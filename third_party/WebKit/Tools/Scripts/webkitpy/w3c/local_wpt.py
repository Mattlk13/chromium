# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A utility class for interacting with a local checkout of the Web Platform Tests."""

import logging

from webkitpy.common.system.executive import ScriptError
from webkitpy.w3c.chromium_commit import ChromiumCommit

WPT_REPO_URL = 'https://chromium.googlesource.com/external/w3c/web-platform-tests.git'
WPT_TMP_DIR = '/tmp/wpt'
CHROMIUM_WPT_DIR = 'third_party/WebKit/LayoutTests/imported/wpt/'

_log = logging.getLogger(__name__)


class LocalWPT(object):

    def __init__(self, host, path=WPT_TMP_DIR, no_fetch=False, use_github=False):
        """
        Args:
            host: A Host object.
            path: Optional, the directory where LocalWPT will check out web-platform-tests.
            no_fetch: Optional, passing true will skip updating the local WPT.
                Intended for use only in development after fetching once.
            use_github: Optional, passing true will check if the GitHub remote is enabled
                (necessary for later pull request steps).
        """
        self.host = host
        self.path = path
        self.branch_name = 'chromium-export-try'

        if no_fetch:
            _log.info('Skipping remote WPT fetch')
            return

        if self.host.filesystem.exists(self.path):
            _log.info('WPT checkout exists at %s, fetching latest', self.path)
            self.run(['git', 'fetch', '--all'])
            self.run(['git', 'checkout', 'origin/master'])
        else:
            _log.info('Cloning %s into %s', WPT_REPO_URL, self.path)
            self.host.executive.run_command(['git', 'clone', WPT_REPO_URL, self.path])

        if use_github and 'github' not in self.run(['git', 'remote']):
            raise Exception('Need to set up remote "github"')

    def run(self, command, **kwargs):
        """Runs a command in the local WPT directory."""
        return self.host.executive.run_command(command, cwd=self.path, **kwargs)

    def most_recent_chromium_commit(self):
        """Finds the most recent commit in WPT with a Chromium commit position."""
        sha = self.run(['git', 'rev-list', 'HEAD', '-n', '1', '--grep=Cr-Commit-Position'])
        if not sha:
            return None, None

        sha = sha.strip()
        position = self.run(['git', 'footers', '--position', sha])
        position = position.strip()
        assert position

        chromium_commit = ChromiumCommit(self.host, position=position)
        return sha, chromium_commit

    def clean(self):
        self.run(['git', 'reset', '--hard', 'HEAD'])
        self.run(['git', 'clean', '-fdx'])
        self.run(['git', 'checkout', 'origin/master'])
        if self.branch_name in self.all_branches():
            self.run(['git', 'branch', '-D', self.branch_name])

    def all_branches(self):
        """Returns a list of local and remote branches."""
        return self.run(['git', 'branch', '-a']).splitlines()

    def create_branch_with_patch(self, message, patch):
        """Commits the given patch and pushes to the upstream repo.

        Args:
            message: Commit message string.
            patch: A patch that can be applied by git apply.
        """
        self.clean()
        all_branches = self.all_branches()

        if self.branch_name in all_branches:
            _log.info('Local branch %s already exists, deleting', self.branch_name)
            self.run(['git', 'branch', '-D', self.branch_name])

        _log.info('Creating local branch %s', self.branch_name)
        self.run(['git', 'checkout', '-b', self.branch_name])

        # Remove Chromium WPT directory prefix.
        patch = patch.replace(CHROMIUM_WPT_DIR, '')

        # TODO(jeffcarp): Use git am -p<n> where n is len(CHROMIUM_WPT_DIR.split(/'))
        # or something not off-by-one.
        self.run(['git', 'apply', '-'], input=patch)
        self.run(['git', 'commit', '-am', message])
        self.run(['git', 'push', 'github', self.branch_name])

        return self.branch_name

    def test_patch(self, patch):
        """Returns the expected output of a patch against origin/master.

        Args:
            patch: The patch to test against.

        Returns:
            A string containing the diff the patch produced.
        """
        self.clean()

        # Remove Chromium WPT directory prefix.
        patch = patch.replace(CHROMIUM_WPT_DIR, '')

        try:
            self.run(['git', 'apply', '-'], input=patch)
            self.run(['git', 'add', '.'])
            output = self.run(['git', 'diff', 'origin/master'])
        except ScriptError as error:
            _log.warning('Patch did not apply cleanly, skipping...')
            output = ''

        self.clean()
        return output

    def commits_behind_master(self, commit):
        """Returns the number of commits after the given commit on origin/master.

        This doesn't include the given commit, and this assumes that the given
        commit is on the the master branch.
        """
        return len(self.run([
            'git', 'rev-list', '{}..origin/master'.format(commit)
        ]).splitlines())
