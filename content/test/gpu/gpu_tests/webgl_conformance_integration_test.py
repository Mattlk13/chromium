# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

from gpu_tests import gpu_integration_test
from gpu_tests import path_util
from gpu_tests import webgl_conformance_expectations
from gpu_tests import webgl2_conformance_expectations

from telemetry.internal.browser import browser_finder

conformance_relcomps = (
  'third_party', 'webgl', 'src', 'sdk', 'tests')

extensions_relcomps = (
    'content', 'test', 'data', 'gpu')

conformance_relpath = os.path.join(*conformance_relcomps)
extensions_relpath = os.path.join(*extensions_relcomps)
conformance_path = os.path.join(path_util.GetChromiumSrcDir(),
                                conformance_relpath)

# These URL prefixes are needed because having more than one static
# server dir is causing the base server directory to be moved up the
# directory hierarchy.
url_prefixes_to_trim = [
  '/'.join(conformance_relcomps) + '/',
  '/'.join(extensions_relcomps) + '/'
]

conformance_harness_script = r"""
  var testHarness = {};
  testHarness._allTestSucceeded = true;
  testHarness._messages = '';
  testHarness._failures = 0;
  testHarness._finished = false;
  testHarness._originalLog = window.console.log;

  testHarness.log = function(msg) {
    testHarness._messages += msg + "\n";
    testHarness._originalLog.apply(window.console, [msg]);
  }

  testHarness.reportResults = function(url, success, msg) {
    testHarness._allTestSucceeded = testHarness._allTestSucceeded && !!success;
    if(!success) {
      testHarness._failures++;
      if(msg) {
        testHarness.log(msg);
      }
    }
  };
  testHarness.notifyFinished = function(url) {
    testHarness._finished = true;
  };
  testHarness.navigateToPage = function(src) {
    var testFrame = document.getElementById("test-frame");
    testFrame.src = src;
  };

  window.webglTestHarness = testHarness;
  window.parent.webglTestHarness = testHarness;
  window.console.log = testHarness.log;
  window.onerror = function(message, url, line) {
    testHarness.reportResults(null, false, message);
    testHarness.notifyFinished(null);
  };
  window.quietMode = function() { return true; }
"""

extension_harness_additional_script = r"""
  window.onload = function() { window._loaded = true; }
"""

def _GenerateTestNameFromTestPath(test_path):
  return ('WebglConformance.%s' %
          test_path.replace('/', '_').replace('-', '_').
          replace('\\', '_').rpartition('.')[0].replace('.', '_'))

def _CompareVersion(version1, version2):
  ver_num1 = [int(x) for x in version1.split('.')]
  ver_num2 = [int(x) for x in version2.split('.')]
  size = min(len(ver_num1), len(ver_num2))
  return cmp(ver_num1[0:size], ver_num2[0:size])


class WebGLConformanceIntegrationTest(gpu_integration_test.GpuIntegrationTest):

  _webgl_version = None

  @classmethod
  def Name(cls):
    return 'webgl_conformance'

  @classmethod
  def AddCommandlineArgs(cls, parser):
    parser.add_option('--webgl-conformance-version',
        help='Version of the WebGL conformance tests to run.',
        default='1.0.4')
    parser.add_option('--webgl2-only',
        help='Whether we include webgl 1 tests if version is 2.0.0 or above.',
        default='false')

  @classmethod
  def GenerateGpuTests(cls, options):
    #
    # Conformance tests
    #
    test_paths = cls._ParseTests(
        '00_test_list.txt',
        options.webgl_conformance_version,
        (options.webgl2_only == 'true'),
        None)
    cls._webgl_version = [
        int(x) for x in options.webgl_conformance_version.split('.')][0]
    for test_path in test_paths:
      # generated test name cannot contain '.'
      name = _GenerateTestNameFromTestPath(test_path).replace(
          '.', '_')
      yield (name,
             os.path.join(conformance_relpath, test_path),
             ('_RunConformanceTest'))

    #
    # Extension tests
    #
    extension_tests = cls._GetExtensionList()
    # Coverage test.
    yield('WebglExtension_TestCoverage',
          os.path.join(extensions_relpath, 'webgl_extension_test.html'),
          ('_RunExtensionCoverageTest',
           extension_tests,
           cls._webgl_version))
    # Individual extension tests.
    for extension in extension_tests:
      yield('WebglExtension_%s' % extension,
            os.path.join(extensions_relpath, 'webgl_extension_test.html'),
            ('_RunExtensionTest',
             extension,
             cls._webgl_version))

  @classmethod
  def _GetExtensionList(cls):
    if cls._webgl_version == 1:
      return [
        'ANGLE_instanced_arrays',
        'EXT_blend_minmax',
        'EXT_disjoint_timer_query',
        'EXT_frag_depth',
        'EXT_shader_texture_lod',
        'EXT_sRGB',
        'EXT_texture_filter_anisotropic',
        'OES_element_index_uint',
        'OES_standard_derivatives',
        'OES_texture_float',
        'OES_texture_float_linear',
        'OES_texture_half_float',
        'OES_texture_half_float_linear',
        'OES_vertex_array_object',
        'WEBGL_compressed_texture_astc',
        'WEBGL_compressed_texture_atc',
        'WEBGL_compressed_texture_etc1',
        'WEBGL_compressed_texture_pvrtc',
        'WEBGL_compressed_texture_s3tc',
        'WEBGL_compressed_texture_s3tc_srgb',
        'WEBGL_debug_renderer_info',
        'WEBGL_debug_shaders',
        'WEBGL_depth_texture',
        'WEBGL_draw_buffers',
        'WEBGL_lose_context',
      ]
    else:
      return [
        'EXT_color_buffer_float',
        'EXT_disjoint_timer_query_webgl2',
        'EXT_texture_filter_anisotropic',
        'OES_texture_float_linear',
        'WEBGL_compressed_texture_astc',
        'WEBGL_compressed_texture_atc',
        'WEBGL_compressed_texture_etc',
        'WEBGL_compressed_texture_etc1',
        'WEBGL_compressed_texture_pvrtc',
        'WEBGL_compressed_texture_s3tc',
        'WEBGL_compressed_texture_s3tc_srgb',
        'WEBGL_debug_renderer_info',
        'WEBGL_debug_shaders',
        'WEBGL_lose_context',
      ]

  def RunActualGpuTest(self, test_path, *args):
    # This indirection allows these tests to trampoline through
    # _RunGpuTest.
    test_name = args[0]
    getattr(self, test_name)(test_path, *args[1:])

  def _NavigateTo(self, test_path, harness_script):
    url = self.UrlOfStaticFilePath(test_path)
    self.tab.Navigate(url, script_to_evaluate_on_commit=harness_script)

  def _CheckTestCompletion(self):
    self.tab.action_runner.WaitForJavaScriptCondition(
        'webglTestHarness._finished', timeout_in_seconds=300)
    if not self._DidWebGLTestSucceed(self.tab):
      self.fail(self._WebGLTestMessages(self.tab))

  def _RunConformanceTest(self, test_path, *args):
    self._NavigateTo(test_path, conformance_harness_script)
    self._CheckTestCompletion()


  def _GetExtensionHarnessScript(self):
    return conformance_harness_script + extension_harness_additional_script

  def _RunExtensionCoverageTest(self, test_path, *args):
    self._NavigateTo(test_path, self._GetExtensionHarnessScript())
    self.tab.action_runner.WaitForJavaScriptCondition(
        'window._loaded', timeout_in_seconds=300)
    extension_list = args[0]
    webgl_version = args[1]
    context_type = "webgl2" if webgl_version == 2 else "webgl"
    extension_list_string = "["
    for extension in extension_list:
      extension_list_string = extension_list_string + extension + ", "
    extension_list_string = extension_list_string + "]"
    self.tab.action_runner.EvaluateJavaScript(
      'checkSupportedExtensions("%s", "%s")' % (
        extension_list_string, context_type))
    self._CheckTestCompletion()

  def _RunExtensionTest(self, test_path, *args):
    self._NavigateTo(test_path, self._GetExtensionHarnessScript())
    self.tab.action_runner.WaitForJavaScriptCondition(
        'window._loaded', timeout_in_seconds=300)
    extension = args[0]
    webgl_version = args[1]
    context_type = "webgl2" if webgl_version == 2 else "webgl"
    self.tab.action_runner.EvaluateJavaScript(
      'checkExtension("%s", "%s")' % (extension, context_type))
    self._CheckTestCompletion()

  @classmethod
  def CustomizeOptions(cls):
    assert cls._webgl_version == 1 or cls._webgl_version == 2
    browser_options = cls._finder_options.browser_options
    # --test-type=gpu is used only to suppress the "Google API Keys are missing"
    # infobar, which causes flakiness in tests.
    browser_options.AppendExtraBrowserArgs([
        '--disable-gesture-requirement-for-media-playback',
        '--disable-domain-blocking-for-3d-apis',
        '--disable-gpu-process-crash-limit',
        '--test-type=gpu',
        '--enable-experimental-canvas-features',
        # Try disabling the GPU watchdog to see if this affects the
        # intermittent GPU process hangs that have been seen on the
        # waterfall. crbug.com/596622 crbug.com/609252
        '--disable-gpu-watchdog'
    ])

    builtin_js_flags = '--js-flags=--expose-gc'
    found_js_flags = False
    user_js_flags = ''
    if browser_options.extra_browser_args:
      for o in browser_options.extra_browser_args:
        if o.startswith('--js-flags'):
          found_js_flags = True
          user_js_flags = o
          break
    if found_js_flags:
      logging.warning('Overriding built-in JavaScript flags:')
      logging.warning(' Original flags: ' + builtin_js_flags)
      logging.warning(' New flags: ' + user_js_flags)
    else:
      browser_options.AppendExtraBrowserArgs([builtin_js_flags])

    if cls._webgl_version == 2:
      browser_options.AppendExtraBrowserArgs([
        '--enable-es3-apis',
      ])
    browser = browser_finder.FindBrowser(browser_options.finder_options)
    if (browser.target_os.startswith('android') and
      browser.browser_type == 'android-webview-shell'):
      # TODO(kbr): this is overly broad. We'd like to do this only on
      # Nexus 9. It'll go away shortly anyway. crbug.com/499928
      #
      # The --ignore_egl_sync_failures is only there to work around
      # some strange failure on the Nexus 9 bot, not reproducible on
      # local hardware.
      browser_options.AppendExtraBrowserArgs([
        '--disable-gl-extensions=GL_EXT_disjoint_timer_query',
        '--ignore_egl_sync_failures',
      ])

  @classmethod
  def _CreateExpectations(cls):
    assert cls._webgl_version == 1 or cls._webgl_version == 2
    if cls._webgl_version == 1:
      return webgl_conformance_expectations.WebGLConformanceExpectations(
          conformance_path, url_prefixes=url_prefixes_to_trim)
    else:
      return webgl2_conformance_expectations.WebGL2ConformanceExpectations(
          conformance_path, url_prefixes=url_prefixes_to_trim)

  @classmethod
  def setUpClass(cls):
    super(cls, WebGLConformanceIntegrationTest).setUpClass()
    cls.CustomizeOptions()
    cls.SetBrowserOptions(cls._finder_options)
    cls.StartBrowser()
    # By setting multiple server directories, the root of the server
    # implicitly becomes the common base directory, i.e., the Chromium
    # src dir, and all URLs have to be specified relative to that.
    cls.SetStaticServerDirs([
      os.path.join(path_util.GetChromiumSrcDir(), conformance_relpath),
      os.path.join(path_util.GetChromiumSrcDir(), extensions_relpath)])

  # Helper functions.

  @staticmethod
  def _DidWebGLTestSucceed(tab):
    return tab.EvaluateJavaScript('webglTestHarness._allTestSucceeded')

  @staticmethod
  def _WebGLTestMessages(tab):
    return tab.EvaluateJavaScript('webglTestHarness._messages')

  @classmethod
  def _ParseTests(cls, path, version, webgl2_only, folder_min_version):
    test_paths = []
    current_dir = os.path.dirname(path)
    full_path = os.path.normpath(os.path.join(conformance_path, path))
    webgl_version = int(version.split('.')[0])

    if not os.path.exists(full_path):
      raise Exception('The WebGL conformance test path specified ' +
        'does not exist: ' + full_path)

    with open(full_path, 'r') as f:
      for line in f:
        line = line.strip()

        if not line:
          continue

        if line.startswith('//') or line.startswith('#'):
          continue

        line_tokens = line.split(' ')
        test_name = line_tokens[-1]

        i = 0
        min_version = None
        max_version = None
        while i < len(line_tokens):
          token = line_tokens[i]
          if token == '--min-version':
            i += 1
            min_version = line_tokens[i]
          elif token == '--max-version':
            i += 1
            max_version = line_tokens[i]
          i += 1

        min_version_to_compare = min_version or folder_min_version

        if (min_version_to_compare and
            _CompareVersion(version, min_version_to_compare) < 0):
          continue

        if max_version and _CompareVersion(version, max_version) > 0:
          continue

        if (webgl2_only and not '.txt' in test_name and
            (not min_version_to_compare or
             not min_version_to_compare.startswith('2'))):
          continue

        if '.txt' in test_name:
          include_path = os.path.join(current_dir, test_name)
          # We only check min-version >= 2.0.0 for the top level list.
          test_paths += cls._ParseTests(
              include_path, version, webgl2_only, min_version_to_compare)
        else:
          test = os.path.join(current_dir, test_name)
          if webgl_version > 1:
            test += '?webglVersion=' + str(webgl_version)
          test_paths.append(test)

    return test_paths
