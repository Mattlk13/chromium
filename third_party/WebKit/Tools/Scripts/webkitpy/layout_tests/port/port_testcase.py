# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Unit testing base class for Port implementations."""

import collections
import errno
import optparse
import socket
import unittest

from webkitpy.common.system.executive_mock import MockExecutive
from webkitpy.common.system.output_capture import OutputCapture
from webkitpy.common.system.system_host import SystemHost
from webkitpy.common.system.system_host_mock import MockSystemHost
from webkitpy.layout_tests.models import test_run_results
from webkitpy.layout_tests.port.base import Port


class FakePrinter(object):

    def write_update(self, msg):
        pass

    def write_throttled_update(self, msg):
        pass


class PortTestCase(unittest.TestCase):
    """Tests that all Port implementations must pass."""
    HTTP_PORTS = (8000, 8080, 8443)
    WEBSOCKET_PORTS = (8880,)

    # Subclasses override this to point to their Port subclass.
    os_name = None
    os_version = None
    port_maker = Port
    port_name = None
    full_port_name = None

    def make_port(self, host=None, port_name=None, options=None, os_name=None, os_version=None, **kwargs):
        host = host or MockSystemHost(os_name=(os_name or self.os_name), os_version=(os_version or self.os_version))
        options = options or optparse.Values({'configuration': 'Release'})
        port_name = port_name or self.port_name
        port_name = self.port_maker.determine_full_port_name(host, options, port_name)
        return self.port_maker(host, port_name, options=options, **kwargs)

    def make_wdiff_available(self, port):
        port._wdiff_available = True

    def test_check_build(self):
        port = self.make_port()
        port._check_file_exists = lambda path, desc: True
        if port._dump_reader:
            port._dump_reader.check_is_functional = lambda: True
        port._options.build = True
        port._check_driver_build_up_to_date = lambda config: True
        port.check_httpd = lambda: True
        oc = OutputCapture()
        try:
            oc.capture_output()
            self.assertEqual(port.check_build(needs_http=True, printer=FakePrinter()),
                             test_run_results.OK_EXIT_STATUS)
        finally:
            _, _, logs = oc.restore_output()
            self.assertIn('pretty patches', logs)         # We should get a warning about PrettyPatch being missing,
            self.assertNotIn('build requirements', logs)  # but not the driver itself.

        port._check_file_exists = lambda path, desc: False
        port._check_driver_build_up_to_date = lambda config: False
        try:
            oc.capture_output()
            self.assertEqual(port.check_build(needs_http=True, printer=FakePrinter()),
                             test_run_results.UNEXPECTED_ERROR_EXIT_STATUS)
        finally:
            _, _, logs = oc.restore_output()
            self.assertIn('pretty patches', logs)        # And, here we should get warnings about both.
            self.assertIn('build requirements', logs)

    def test_default_batch_size(self):
        port = self.make_port()

        # Test that we set a finite batch size for sanitizer builds.
        port._options.enable_sanitizer = True
        sanitized_batch_size = port.default_batch_size()
        self.assertIsNotNone(sanitized_batch_size)

    def test_default_child_processes(self):
        port = self.make_port()
        num_workers = port.default_child_processes()
        self.assertGreaterEqual(num_workers, 1)

    def test_default_max_locked_shards(self):
        port = self.make_port()
        port.default_child_processes = lambda: 16
        self.assertEqual(port.default_max_locked_shards(), 4)
        port.default_child_processes = lambda: 2
        self.assertEqual(port.default_max_locked_shards(), 1)

    def test_default_timeout_ms(self):
        self.assertEqual(self.make_port(options=optparse.Values({'configuration': 'Release'})).default_timeout_ms(), 6000)
        self.assertEqual(self.make_port(options=optparse.Values({'configuration': 'Debug'})).default_timeout_ms(), 18000)

    def test_default_pixel_tests(self):
        self.assertEqual(self.make_port().default_pixel_tests(), True)

    def test_driver_cmd_line(self):
        port = self.make_port()
        self.assertTrue(len(port.driver_cmd_line()))

        options = optparse.Values(dict(additional_driver_flag=['--foo=bar', '--foo=baz']))
        port = self.make_port(options=options)
        cmd_line = port.driver_cmd_line()
        self.assertTrue('--foo=bar' in cmd_line)
        self.assertTrue('--foo=baz' in cmd_line)

    def assert_servers_are_down(self, host, ports):
        for port in ports:
            try:
                test_socket = socket.socket()
                test_socket.connect((host, port))
                self.fail()
            except IOError as error:
                self.assertTrue(error.errno in (errno.ECONNREFUSED, errno.ECONNRESET))
            finally:
                test_socket.close()

    def assert_servers_are_up(self, host, ports):
        for port in ports:
            try:
                test_socket = socket.socket()
                test_socket.connect((host, port))
            except IOError:
                self.fail('failed to connect to %s:%d' % (host, port))
            finally:
                test_socket.close()

    def test_diff_image__missing_both(self):
        port = self.make_port()
        self.assertEqual(port.diff_image(None, None), (None, None))
        self.assertEqual(port.diff_image(None, ''), (None, None))
        self.assertEqual(port.diff_image('', None), (None, None))

        self.assertEqual(port.diff_image('', ''), (None, None))

    def test_diff_image__missing_actual(self):
        port = self.make_port()
        self.assertEqual(port.diff_image(None, 'foo'), ('foo', None))
        self.assertEqual(port.diff_image('', 'foo'), ('foo', None))

    def test_diff_image__missing_expected(self):
        port = self.make_port()
        self.assertEqual(port.diff_image('foo', None), ('foo', None))
        self.assertEqual(port.diff_image('foo', ''), ('foo', None))

    def test_diff_image(self):

        def _path_to_image_diff():
            return "/path/to/image_diff"

        port = self.make_port()
        port._path_to_image_diff = _path_to_image_diff

        mock_image_diff = "MOCK Image Diff"

        def mock_run_command(args):
            port.host.filesystem.write_binary_file(args[4], mock_image_diff)
            return 1

        # Images are different.
        port._executive = MockExecutive(run_command_fn=mock_run_command)  # pylint: disable=protected-access
        self.assertEqual(mock_image_diff, port.diff_image("EXPECTED", "ACTUAL")[0])

        # Images are the same.
        port._executive = MockExecutive(exit_code=0)  # pylint: disable=protected-access
        self.assertEqual(None, port.diff_image("EXPECTED", "ACTUAL")[0])

        # There was some error running image_diff.
        port._executive = MockExecutive(exit_code=2)  # pylint: disable=protected-access
        exception_raised = False
        try:
            port.diff_image("EXPECTED", "ACTUAL")
        except ValueError:
            exception_raised = True
        self.assertFalse(exception_raised)

    def test_diff_image_crashed(self):
        port = self.make_port()
        port._executive = MockExecutive(exit_code=2)  # pylint: disable=protected-access
        self.assertEqual(port.diff_image("EXPECTED", "ACTUAL"),
                         (None, 'Image diff returned an exit code of 2. See http://crbug.com/278596'))

    def test_check_wdiff(self):
        port = self.make_port()
        port.check_wdiff()

    def test_wdiff_text_fails(self):
        host = MockSystemHost(os_name=self.os_name, os_version=self.os_version)
        host.executive = MockExecutive(should_throw=True)  # pylint: disable=protected-access
        port = self.make_port(host=host)
        port._executive = host.executive  # AndroidPortTest.make_port sets its own executive, so reset that as well.

        # This should raise a ScriptError that gets caught and turned into the
        # error text, and also mark wdiff as not available.
        self.make_wdiff_available(port)
        self.assertTrue(port.wdiff_available())
        diff_txt = port.wdiff_text("/tmp/foo.html", "/tmp/bar.html")
        self.assertEqual(diff_txt, port._wdiff_error_html)
        self.assertFalse(port.wdiff_available())

    def test_test_configuration(self):
        port = self.make_port()
        self.assertTrue(port.test_configuration())

    def test_get_crash_log(self):
        port = self.make_port()
        self.assertEqual(port._get_crash_log(None, None, None, None, newer_than=None),
                         (None,
                          'crash log for <unknown process name> (pid <unknown>):\n'
                          'STDOUT: <empty>\n'
                          'STDERR: <empty>\n'))

        self.assertEqual(port._get_crash_log('foo', 1234, 'out bar\nout baz', 'err bar\nerr baz\n', newer_than=None),
                         ('err bar\nerr baz\n',
                          'crash log for foo (pid 1234):\n'
                          'STDOUT: out bar\n'
                          'STDOUT: out baz\n'
                          'STDERR: err bar\n'
                          'STDERR: err baz\n'))

        self.assertEqual(port._get_crash_log('foo', 1234, 'foo\xa6bar', 'foo\xa6bar', newer_than=None),
                         ('foo\xa6bar',
                          u'crash log for foo (pid 1234):\n'
                          u'STDOUT: foo\ufffdbar\n'
                          u'STDERR: foo\ufffdbar\n'))

        self.assertEqual(port._get_crash_log('foo', 1234, 'foo\xa6bar', 'foo\xa6bar', newer_than=1.0),
                         ('foo\xa6bar',
                          u'crash log for foo (pid 1234):\n'
                          u'STDOUT: foo\ufffdbar\n'
                          u'STDERR: foo\ufffdbar\n'))

    def assert_build_path(self, options, dirs, expected_path):
        port = self.make_port(options=options)
        for directory in dirs:
            port.host.filesystem.maybe_make_directory(directory)
        self.assertEqual(port._build_path(), expected_path)

    def test_expectations_files(self):
        port = self.make_port()
        self.assertEqual(port.expectations_files(), [
            port.path_to_generic_test_expectations_file(),
            port.host.filesystem.join(port.layout_tests_dir(), 'NeverFixTests'),
            port.host.filesystem.join(port.layout_tests_dir(), 'StaleTestExpectations'),
            port.host.filesystem.join(port.layout_tests_dir(), 'SlowTests'),
        ])

    def test_expectations_files_wptserve_enabled(self):
        port = self.make_port(options=optparse.Values(dict(enable_wptserve=True)))
        self.assertEqual(port.expectations_files(), [
            port.path_to_generic_test_expectations_file(),
            port.host.filesystem.join(port.layout_tests_dir(), 'NeverFixTests'),
            port.host.filesystem.join(port.layout_tests_dir(), 'StaleTestExpectations'),
            port.host.filesystem.join(port.layout_tests_dir(), 'SlowTests'),
            port.host.filesystem.join(port.layout_tests_dir(), 'WPTServeExpectations'),
        ])

    def test_check_sys_deps(self):
        port = self.make_port()
        port._executive = MockExecutive(exit_code=0)  # pylint: disable=protected-access
        self.assertEqual(port.check_sys_deps(needs_http=False), test_run_results.OK_EXIT_STATUS)
        port._executive = MockExecutive(exit_code=1, output='testing output failure')  # pylint: disable=protected-access
        self.assertEqual(port.check_sys_deps(needs_http=False), test_run_results.SYS_DEPS_EXIT_STATUS)

    def test_expectations_ordering(self):
        port = self.make_port()
        for path in port.expectations_files():
            port.host.filesystem.write_text_file(path, '')
        ordered_dict = port.expectations_dict()
        self.assertEqual(port.path_to_generic_test_expectations_file(), ordered_dict.keys()[0])

        options = optparse.Values(dict(additional_expectations=['/tmp/foo', '/tmp/bar']))
        port = self.make_port(options=options)
        for path in port.expectations_files():
            port.host.filesystem.write_text_file(path, '')
        port.host.filesystem.write_text_file('/tmp/foo', 'foo')
        port.host.filesystem.write_text_file('/tmp/bar', 'bar')
        ordered_dict = port.expectations_dict()
        self.assertEqual(ordered_dict.keys()[-2:], options.additional_expectations)
        self.assertEqual(ordered_dict.values()[-2:], ['foo', 'bar'])

    def test_path_to_apache_config_file(self):
        # Specific behavior may vary by port, so unit test sub-classes may override this.
        port = self.make_port()

        port.host.environ['WEBKIT_HTTP_SERVER_CONF_PATH'] = '/path/to/httpd.conf'
        self.assertRaises(IOError, port.path_to_apache_config_file)
        port.host.filesystem.write_text_file('/existing/httpd.conf', 'Hello, world!')
        port.host.environ['WEBKIT_HTTP_SERVER_CONF_PATH'] = '/existing/httpd.conf'
        self.assertEqual(port.path_to_apache_config_file(), '/existing/httpd.conf')

        # Mock out _apache_config_file_name_for_platform to avoid mocking platform info.
        port._apache_config_file_name_for_platform = lambda: 'httpd.conf'
        del port.host.environ['WEBKIT_HTTP_SERVER_CONF_PATH']
        self.assertEqual(
            port.path_to_apache_config_file(),
            port.host.filesystem.join(port.apache_config_directory(), 'httpd.conf'))

        # Check that even if we mock out _apache_config_file_name, the environment variable takes precedence.
        port.host.environ['WEBKIT_HTTP_SERVER_CONF_PATH'] = '/existing/httpd.conf'
        self.assertEqual(port.path_to_apache_config_file(), '/existing/httpd.conf')

    def test_additional_platform_directory(self):
        port = self.make_port(options=optparse.Values(dict(additional_platform_directory=['/tmp/foo'])))
        self.assertEqual(port.baseline_search_path()[0], '/tmp/foo')

    def test_virtual_test_suites(self):
        # We test that we can load the real LayoutTests/VirtualTestSuites file properly, so we
        # use a real SystemHost(). We don't care what virtual_test_suites() returns as long
        # as it is iterable.
        port = self.make_port(host=SystemHost(), port_name=self.full_port_name)
        self.assertTrue(isinstance(port.virtual_test_suites(), collections.Iterable))
