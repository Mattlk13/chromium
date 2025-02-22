# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from webkitpy.common.system.executive import Executive, ScriptError
from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.filesystem import FileSystem
from webkitpy.common.system.filesystem_mock import MockFileSystem
from webkitpy.common.checkout.scm.detection import detect_scm_system
from webkitpy.common.checkout.scm.git import Git


class GitTestWithRealFilesystemAndExecutive(unittest.TestCase):

    def setUp(self):
        self.executive = Executive()
        self.filesystem = FileSystem()
        self.original_cwd = self.filesystem.getcwd()

        # Set up fresh git repository with one commit.
        self.untracking_checkout_path = self._mkdtemp(suffix='-git_unittest_untracking')
        self._run(['git', 'init', self.untracking_checkout_path])
        self._chdir(self.untracking_checkout_path)
        self._write_text_file('foo_file', 'foo')
        self._run(['git', 'add', 'foo_file'])
        self._run(['git', 'commit', '-am', 'dummy commit'])
        self.untracking_scm = detect_scm_system(self.untracking_checkout_path)

        # Then set up a second git repo that tracks the first one.
        self.tracking_git_checkout_path = self._mkdtemp(suffix='-git_unittest_tracking')
        self._run(['git', 'clone', '--quiet', self.untracking_checkout_path, self.tracking_git_checkout_path])
        self._chdir(self.tracking_git_checkout_path)
        self.tracking_scm = detect_scm_system(self.tracking_git_checkout_path)

    def tearDown(self):
        self._chdir(self.original_cwd)
        self._run(['rm', '-rf', self.tracking_git_checkout_path])
        self._run(['rm', '-rf', self.untracking_checkout_path])

    def _join(self, *comps):
        return self.filesystem.join(*comps)

    def _chdir(self, path):
        self.filesystem.chdir(path)

    def _mkdir(self, path):
        assert not self.filesystem.exists(path)
        self.filesystem.maybe_make_directory(path)

    def _mkdtemp(self, **kwargs):
        return str(self.filesystem.mkdtemp(**kwargs))

    def _remove(self, path):
        self.filesystem.remove(path)

    def _run(self, *args, **kwargs):
        return self.executive.run_command(*args, **kwargs)

    def _run_silent(self, args, **kwargs):
        self.executive.run_command(args, **kwargs)

    def _write_text_file(self, path, contents):
        self.filesystem.write_text_file(path, contents)

    def _write_binary_file(self, path, contents):
        self.filesystem.write_binary_file(path, contents)

    def _make_diff(self, command, *args):
        # We use this wrapper to disable output decoding. diffs should be treated as
        # binary files since they may include text files of multiple different encodings.
        return self._run([command, 'diff'] + list(args), decode_output=False)

    def _git_diff(self, *args):
        return self._make_diff('git', *args)

    def test_add_list(self):
        self._chdir(self.untracking_checkout_path)
        git = self.untracking_scm
        self._mkdir('added_dir')
        self._write_text_file('added_dir/added_file', 'new stuff')
        print self._run(['ls', 'added_dir'])
        print self._run(['pwd'])
        print self._run(['cat', 'added_dir/added_file'])
        git.add_list(['added_dir/added_file'])
        self.assertIn('added_dir/added_file', git.added_files())

    def test_delete_recursively(self):
        self._chdir(self.untracking_checkout_path)
        git = self.untracking_scm
        self._mkdir('added_dir')
        self._write_text_file('added_dir/added_file', 'new stuff')
        git.add_list(['added_dir/added_file'])
        self.assertIn('added_dir/added_file', git.added_files())
        git.delete_list(['added_dir/added_file'])
        self.assertNotIn('added_dir', git.added_files())

    def test_delete_recursively_or_not(self):
        self._chdir(self.untracking_checkout_path)
        git = self.untracking_scm
        self._mkdir('added_dir')
        self._write_text_file('added_dir/added_file', 'new stuff')
        self._write_text_file('added_dir/another_added_file', 'more new stuff')
        git.add_list(['added_dir/added_file', 'added_dir/another_added_file'])
        self.assertIn('added_dir/added_file', git.added_files())
        self.assertIn('added_dir/another_added_file', git.added_files())
        git.delete_list(['added_dir/added_file'])
        self.assertIn('added_dir/another_added_file', git.added_files())

    def test_exists(self):
        self._chdir(self.untracking_checkout_path)
        git = self.untracking_scm
        self._chdir(git.checkout_root)
        self.assertFalse(git.exists('foo.txt'))
        self._write_text_file('foo.txt', 'some stuff')
        self.assertFalse(git.exists('foo.txt'))
        git.add_list(['foo.txt'])
        git.commit_locally_with_message('adding foo')
        self.assertTrue(git.exists('foo.txt'))
        git.delete_list(['foo.txt'])
        git.commit_locally_with_message('deleting foo')
        self.assertFalse(git.exists('foo.txt'))

    def test_move(self):
        self._chdir(self.untracking_checkout_path)
        git = self.untracking_scm
        self._write_text_file('added_file', 'new stuff')
        git.add_list(['added_file'])
        git.move('added_file', 'moved_file')
        self.assertIn('moved_file', git.added_files())

    def test_move_recursive(self):
        self._chdir(self.untracking_checkout_path)
        git = self.untracking_scm
        self._mkdir('added_dir')
        self._write_text_file('added_dir/added_file', 'new stuff')
        self._write_text_file('added_dir/another_added_file', 'more new stuff')
        git.add_list(['added_dir'])
        git.move('added_dir', 'moved_dir')
        self.assertIn('moved_dir/added_file', git.added_files())
        self.assertIn('moved_dir/another_added_file', git.added_files())

    def test_remote_branch_ref(self):
        # This tests a protected method. pylint: disable=protected-access
        self.assertEqual(self.tracking_scm._remote_branch_ref(), 'refs/remotes/origin/master')
        self._chdir(self.untracking_checkout_path)
        self.assertRaises(ScriptError, self.untracking_scm._remote_branch_ref)

    def test_create_patch(self):
        self._chdir(self.tracking_git_checkout_path)
        git = self.tracking_scm
        self._write_text_file('test_file_commit1', 'contents')
        self._run(['git', 'add', 'test_file_commit1'])
        git.commit_locally_with_message('message')
        git._patch_order = lambda: ''  # pylint: disable=protected-access
        patch = git.create_patch()
        self.assertNotRegexpMatches(patch, r'Subversion Revision:')

    def test_patches_have_filenames_with_prefixes(self):
        self._chdir(self.tracking_git_checkout_path)
        git = self.tracking_scm
        self._write_text_file('test_file_commit1', 'contents')
        self._run(['git', 'add', 'test_file_commit1'])
        git.commit_locally_with_message('message')

        # Even if diff.noprefix is enabled, create_patch() produces diffs with prefixes.
        self._run(['git', 'config', 'diff.noprefix', 'true'])
        git._patch_order = lambda: ''  # pylint: disable=protected-access
        patch = git.create_patch()
        self.assertRegexpMatches(patch, r'^diff --git a/test_file_commit1 b/test_file_commit1')

    def test_rename_files(self):
        self._chdir(self.tracking_git_checkout_path)
        git = self.tracking_scm
        git.move('foo_file', 'bar_file')
        git.commit_locally_with_message('message')

    def test_commit_position_from_git_log(self):
        # This tests a protected method. pylint: disable=protected-access
        git_log = """
commit 624c3081c0
Author: foobarbaz1 <foobarbaz1@chromium.org>
Date:   Mon Sep 28 19:10:30 2015 -0700

    Test foo bar baz qux 123.

    BUG=000000

    Review URL: https://codereview.chromium.org/999999999

    Cr-Commit-Position: refs/heads/master@{#1234567}
"""
        self._chdir(self.tracking_git_checkout_path)
        git = self.tracking_scm
        self.assertEqual(git._commit_position_from_git_log(git_log), 1234567)

    def test_timestamp_of_revision(self):
        # This tests a protected method. pylint: disable=protected-access
        self._chdir(self.tracking_git_checkout_path)
        git = self.tracking_scm
        position_regex = git._commit_position_regex_for_timestamp()
        git.most_recent_log_matching(position_regex, git.checkout_root)


class GitTestWithMock(unittest.TestCase):

    def make_scm(self):
        git = Git(cwd='.', executive=MockExecutive(), filesystem=MockFileSystem())
        git.read_git_config = lambda *args, **kw: 'MOCKKEY:MOCKVALUE'
        return git

    def _assert_timestamp_of_revision(self, canned_git_output, expected):
        git = self.make_scm()
        git.find_checkout_root = lambda path: ''
        # Modifying protected method. pylint: disable=protected-access
        git._run_git = lambda args: canned_git_output
        self.assertEqual(git.timestamp_of_revision('some-path', '12345'), expected)

    def test_timestamp_of_revision_utc(self):
        self._assert_timestamp_of_revision('Date: 2013-02-08 08:05:49 +0000', '2013-02-08T08:05:49Z')

    def test_timestamp_of_revision_positive_timezone(self):
        self._assert_timestamp_of_revision('Date: 2013-02-08 01:02:03 +0130', '2013-02-07T23:32:03Z')

    def test_timestamp_of_revision_pacific_timezone(self):
        self._assert_timestamp_of_revision('Date: 2013-02-08 01:55:21 -0800', '2013-02-08T09:55:21Z')

    def test_unstaged_files(self):
        scm = self.make_scm()
        status_lines = [
            ' M d/modified.txt',
            ' D d/deleted.txt',
            '?? d/untracked.txt',
            'D  d/deleted.txt',
            'M  d/modified-staged.txt',
            'A  d/added-staged.txt',
        ]
        # pylint: disable=protected-access
        scm._run_git = lambda args: '\x00'.join(status_lines) + '\x00'
        self.assertEqual(
            scm.unstaged_changes(),
            {
                'd/modified.txt': 'M',
                'd/deleted.txt': 'D',
                'd/untracked.txt': '?',
            })
