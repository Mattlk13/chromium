// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/readback_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/ppapi_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/window_open_disposition.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/point.h"

namespace {

// Use fixed browser dimensions for pixel tests.
const int kBrowserWidth = 600;
const int kBrowserHeight = 700;

// Compare only a portion of the snapshots, as different platforms will
// capture different sized snapshots (due to differences in browser chrome).
const int kComparisonWidth = 500;
const int kComparisonHeight = 600;

// Different platforms have slightly different pixel output, due to different
// graphics implementations. Slightly different pixels (in BGR space) are still
// counted as a matching pixel by this simple manhattan distance threshold.
const int kPixelManhattanDistanceTolerance = 25;

std::string RunTestScript(base::StringPiece test_script,
                          content::WebContents* contents,
                          const std::string& element_id) {
  std::string script = base::StringPrintf(
      "var plugin = window.document.getElementById('%s');"
      "if (plugin === undefined ||"
      "    (plugin.nodeName !== 'OBJECT' && plugin.nodeName !== 'EMBED')) {"
      "  window.domAutomationController.send('error');"
      "} else {"
      "  %s"
      "}",
      element_id.c_str(), test_script.data());
  std::string result;
  EXPECT_TRUE(
      content::ExecuteScriptAndExtractString(contents, script, &result));
  return result;
}

// This also tests that we have JavaScript access to the underlying plugin.
bool PluginLoaded(content::WebContents* contents,
                  const std::string& element_id) {
  std::string result = RunTestScript(
      "if (plugin.postMessage === undefined) {"
      "  window.domAutomationController.send('poster_only');"
      "} else {"
      "  window.domAutomationController.send('plugin_loaded');"
      "}",
      contents, element_id);
  EXPECT_NE("error", result);
  return result == "plugin_loaded";
}

// Blocks until the placeholder is ready.
void WaitForPlaceholderReady(content::WebContents* contents,
                             const std::string& element_id) {
  std::string result = RunTestScript(
      "function handleEvent(event) {"
      "  if (event.data === 'placeholderReady') {"
      "    window.domAutomationController.send('ready');"
      "    plugin.removeEventListener('message', handleEvent);"
      "  }"
      "}"
      "plugin.addEventListener('message', handleEvent);"
      "if (plugin.hasAttribute('placeholderReady')) {"
      "  window.domAutomationController.send('ready');"
      "  plugin.removeEventListener('message', handleEvent);"
      "}",
      contents, element_id);
  ASSERT_EQ("ready", result);
}

// Also waits for the placeholder UI overlay to finish loading.
void VerifyPluginIsThrottled(content::WebContents* contents,
                             const std::string& element_id) {
  std::string result = RunTestScript(
      "function handleEvent(event) {"
      "  if (event.data.isPeripheral && event.data.isThrottled && "
      "      event.data.isHiddenForPlaceholder) {"
      "    window.domAutomationController.send('throttled');"
      "    plugin.removeEventListener('message', handleEvent);"
      "  }"
      "}"
      "plugin.addEventListener('message', handleEvent);"
      "if (plugin.postMessage !== undefined) {"
      "  plugin.postMessage('getPowerSaverStatus');"
      "}",
      contents, element_id);
  EXPECT_EQ("throttled", result);

  // Page should continue to have JavaScript access to all throttled plugins.
  EXPECT_TRUE(PluginLoaded(contents, element_id));

  WaitForPlaceholderReady(contents, element_id);
}

void VerifyPluginMarkedEssential(content::WebContents* contents,
                                 const std::string& element_id) {
  std::string result = RunTestScript(
      "function handleEvent(event) {"
      "  if (event.data.isPeripheral === false) {"
      "    window.domAutomationController.send('essential');"
      "    plugin.removeEventListener('message', handleEvent);"
      "  }"
      "}"
      "plugin.addEventListener('message', handleEvent);"
      "if (plugin.postMessage !== undefined) {"
      "  plugin.postMessage('getPowerSaverStatus');"
      "}",
      contents, element_id);
  EXPECT_EQ("essential", result);
  EXPECT_TRUE(PluginLoaded(contents, element_id));
}

void VerifyVisualStateUpdated(const base::Closure& done_cb,
                              bool visual_state_updated) {
  ASSERT_TRUE(visual_state_updated);
  done_cb.Run();
}

bool SnapshotMatches(const base::FilePath& reference, const SkBitmap& bitmap) {
  if (bitmap.width() < kComparisonWidth ||
      bitmap.height() < kComparisonHeight) {
    return false;
  }

  std::string reference_data;
  if (!base::ReadFileToString(reference, &reference_data))
    return false;

  int w = 0;
  int h = 0;
  std::vector<unsigned char> decoded;
  if (!gfx::PNGCodec::Decode(reinterpret_cast<unsigned char*>(
                                 base::string_as_array(&reference_data)),
                             reference_data.size(), gfx::PNGCodec::FORMAT_BGRA,
                             &decoded, &w, &h)) {
    return false;
  }

  if (w < kComparisonWidth || h < kComparisonHeight)
    return false;

  int32_t* ref_pixels = reinterpret_cast<int32_t*>(decoded.data());
  SkAutoLockPixels lock_image(bitmap);
  int32_t* pixels = static_cast<int32_t*>(bitmap.getPixels());

  bool success = true;
  for (int y = 0; y < kComparisonHeight; ++y) {
    for (int x = 0; x < kComparisonWidth; ++x) {
      int32_t pixel = pixels[y * bitmap.rowBytes() / sizeof(int32_t) + x];
      int pixel_b = pixel & 0xFF;
      int pixel_g = (pixel >> 8) & 0xFF;
      int pixel_r = (pixel >> 16) & 0xFF;

      int32_t ref_pixel = ref_pixels[y * w + x];
      int ref_pixel_b = ref_pixel & 0xFF;
      int ref_pixel_g = (ref_pixel >> 8) & 0xFF;
      int ref_pixel_r = (ref_pixel >> 16) & 0xFF;

      int manhattan_distance = abs(pixel_b - ref_pixel_b) +
                               abs(pixel_g - ref_pixel_g) +
                               abs(pixel_r - ref_pixel_r);

      if (manhattan_distance > kPixelManhattanDistanceTolerance) {
        ADD_FAILURE() << "Pixel test failed on (" << x << ", " << y << "). " <<
            "Pixel manhattan distance: " << manhattan_distance << ".";
        success = false;
      }
    }
  }

  return success;
}

// |snapshot_matches| is set to true if the snapshot matches the reference and
// the test passes. Otherwise, set to false.
void CompareSnapshotToReference(const base::FilePath& reference,
                                bool* snapshot_matches,
                                const base::Closure& done_cb,
                                const SkBitmap& bitmap,
                                content::ReadbackResponse response) {
  DCHECK(snapshot_matches);
  ASSERT_EQ(content::READBACK_SUCCESS, response);

  *snapshot_matches = SnapshotMatches(reference, bitmap);

  // When rebaselining the pixel test, the test may fail. However, the
  // reference file will still be overwritten.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRebaselinePixelTests)) {
    SkBitmap clipped_bitmap;
    bitmap.extractSubset(&clipped_bitmap,
                         SkIRect::MakeWH(kComparisonWidth, kComparisonHeight));
    std::vector<unsigned char> png_data;
    ASSERT_TRUE(
        gfx::PNGCodec::EncodeBGRASkBitmap(clipped_bitmap, false, &png_data));
    ASSERT_EQ(static_cast<int>(png_data.size()),
              base::WriteFile(reference,
                              reinterpret_cast<const char*>(png_data.data()),
                              png_data.size()));
  }

  done_cb.Run();
}

}  // namespace

class PluginPowerSaverBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    if (PixelTestsEnabled())
      EnablePixelOutput();

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    embedded_test_server()->ServeFilesFromDirectory(
        ui_test_utils::GetTestFilePath(
            base::FilePath(FILE_PATH_LITERAL("plugin_power_saver")),
            base::FilePath()));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnablePluginPlaceholderTesting);
    ASSERT_TRUE(ppapi::RegisterFlashTestPlugin(command_line));

    // Allows us to use the same reference image on HiDPI/Retina displays.
    command_line->AppendSwitchASCII(switches::kForceDeviceScaleFactor, "1");

    // The pixel tests run more reliably in software mode.
    if (PixelTestsEnabled())
      command_line->AppendSwitch(switches::kDisableGpu);
  }

  void SetUpInProcessBrowserTestFixture() override {
    // Disable HTML5 By Default feature to test Plugin Power Saver specifically.
    feature_list.InitAndDisableFeature(features::kPreferHtmlOverPlugins);
  }

 protected:
  void LoadHTML(const std::string& file) {
    if (PixelTestsEnabled()) {
      gfx::Rect bounds(gfx::Rect(0, 0, kBrowserWidth, kBrowserHeight));
      gfx::Rect screen_bounds =
          display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
      ASSERT_GT(screen_bounds.width(), kBrowserWidth);
      ASSERT_GT(screen_bounds.height(), kBrowserHeight);
      browser()->window()->SetBounds(bounds);
    }

    ui_test_utils::NavigateToURL(browser(),
                                 embedded_test_server()->GetURL(file));
    EXPECT_TRUE(content::WaitForRenderFrameReady(
        GetActiveWebContents()->GetMainFrame()));
  }

  // Loads a peripheral plugin (small cross origin) named 'plugin'.
  void LoadPeripheralPlugin() { LoadHTML("/load_peripheral_plugin.html"); }

  // Returns the background WebContents.
  content::WebContents* LoadHTMLInBackgroundTab(const std::string& file) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), embedded_test_server()->GetURL(file),
        WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

    int index = browser()->tab_strip_model()->GetIndexOfLastWebContentsOpenedBy(
        GetActiveWebContents(), 0 /* start_index */);
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(index);
    EXPECT_TRUE(content::WaitForRenderFrameReady(contents->GetMainFrame()));
    return contents;
  }

  void ActivateTab(content::WebContents* contents) {
    browser()->tab_strip_model()->ActivateTabAt(
        browser()->tab_strip_model()->GetIndexOfWebContents(contents),
        true /* user_gesture */);
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // This sends a simulated click at |point| and waits for test plugin to send
  // a status message indicating that it is essential. The test plugin sends a
  // status message during:
  //  - Plugin creation, to handle a plugin freshly created from a poster.
  //  - Peripheral status change.
  //  - In response to the explicit 'getPowerSaverStatus' request, in case the
  //    test has missed the above two events.
  void SimulateClickAndAwaitMarkedEssential(const std::string& element_id,
                                            const gfx::Point& point) {
    WaitForPlaceholderReady(GetActiveWebContents(), element_id);
    content::SimulateMouseClickAt(GetActiveWebContents(), 0 /* modifiers */,
                                  blink::WebMouseEvent::Button::Left, point);

    VerifyPluginMarkedEssential(GetActiveWebContents(), element_id);
  }

  // |element_id| must be an element on the foreground tab.
  void VerifyPluginIsPlaceholderOnly(const std::string& element_id) {
    EXPECT_FALSE(PluginLoaded(GetActiveWebContents(), element_id));
    WaitForPlaceholderReady(GetActiveWebContents(), element_id);
  }

  bool VerifySnapshot(const base::FilePath::StringType& expected_filename) {
    if (!PixelTestsEnabled())
      return true;

    base::FilePath reference = ui_test_utils::GetTestFilePath(
        base::FilePath(FILE_PATH_LITERAL("plugin_power_saver")),
        base::FilePath(expected_filename));

    GetActiveWebContents()->GetMainFrame()->InsertVisualStateCallback(
        base::Bind(&VerifyVisualStateUpdated,
                   base::MessageLoop::QuitWhenIdleClosure()));
    content::RunMessageLoop();

    content::RenderWidgetHost* rwh =
        GetActiveWebContents()->GetRenderViewHost()->GetWidget();

    if (!rwh->CanCopyFromBackingStore()) {
      ADD_FAILURE() << "Could not copy from backing store.";
      return false;
    }

    bool snapshot_matches = false;
    rwh->CopyFromBackingStore(
        gfx::Rect(), gfx::Size(),
        base::Bind(&CompareSnapshotToReference, reference, &snapshot_matches,
                   base::MessageLoop::QuitWhenIdleClosure()),
        kN32_SkColorType);

    content::RunMessageLoop();

    return snapshot_matches;
  }

  // TODO(tommycli): Remove this once all flakiness resolved.
  bool PixelTestsEnabled() {
#if defined(OS_WIN) || defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
    // Flaky on Windows, Asan, and Msan bots.
    // See crbug.com/549285 and crbug.com/512140.
    return false;
#elif defined(OS_CHROMEOS)
    // Because ChromeOS cannot use software rendering and the pixel tests
    // continue to flake with hardware acceleration, disable these on ChromeOS.
    return false;
#else
    return true;
#endif
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, EssentialPlugins) {
  LoadHTML("/essential_plugins.html");

  VerifyPluginMarkedEssential(GetActiveWebContents(), "small_same_origin");
  VerifyPluginMarkedEssential(GetActiveWebContents(),
                              "small_same_origin_poster");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "large_cross_origin");
  VerifyPluginMarkedEssential(GetActiveWebContents(),
                              "medium_16_9_cross_origin");
}

// This test fail on macOS 10.12. https://crbug.com/599484.
#if defined(OS_MACOSX)
#define MAYBE_SmallCrossOrigin DISABLED_SmallCrossOrigin
#else
#define MAYBE_SmallCrossOrigin SmallCrossOrigin
#endif
IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, MAYBE_SmallCrossOrigin) {
  LoadHTML("/small_cross_origin.html");

  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin");
  VerifyPluginIsPlaceholderOnly("plugin_poster");

  EXPECT_TRUE(
      VerifySnapshot(FILE_PATH_LITERAL("small_cross_origin_expected.png")));

  SimulateClickAndAwaitMarkedEssential("plugin", gfx::Point(50, 50));
  SimulateClickAndAwaitMarkedEssential("plugin_poster", gfx::Point(50, 150));
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, ContentSettings) {
  HostContentSettingsMap* content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());

  // Throttle on DETECT.
  content_settings_map->SetDefaultContentSetting(
      CONTENT_SETTINGS_TYPE_PLUGINS, CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);
  LoadPeripheralPlugin();
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin");

  // Don't throttle on ALLOW.
  content_settings_map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                                 CONTENT_SETTING_ALLOW);
  LoadPeripheralPlugin();
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, SmallerThanPlayIcon) {
  LoadHTML("/smaller_than_play_icon.html");

  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin_16");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin_32");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin_16_64");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin_64_16");

  EXPECT_TRUE(
      VerifySnapshot(FILE_PATH_LITERAL("smaller_than_play_icon_expected.png")));
}

// This test fail on macOS 10.12. https://crbug.com/599484.
#if defined(OS_MACOSX)
#define MAYBE_PosterTests DISABLED_PosterTests
#else
#define MAYBE_PosterTests PosterTests
#endif
IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, MAYBE_PosterTests) {
  // This test simultaneously verifies the varied supported poster syntaxes,
  // as well as verifies that the poster is rendered correctly with various
  // mismatched aspect ratios and sizes, following the same rules as VIDEO.
  LoadHTML("/poster_tests.html");

  VerifyPluginIsPlaceholderOnly("plugin_src");
  VerifyPluginIsPlaceholderOnly("plugin_srcset");

  VerifyPluginIsPlaceholderOnly("plugin_poster_param");
  VerifyPluginIsPlaceholderOnly("plugin_embed_src");
  VerifyPluginIsPlaceholderOnly("plugin_embed_srcset");

  VerifyPluginIsPlaceholderOnly("poster_missing");
  VerifyPluginIsPlaceholderOnly("poster_too_small");
  VerifyPluginIsPlaceholderOnly("poster_too_big");

  VerifyPluginIsPlaceholderOnly("poster_16");
  VerifyPluginIsPlaceholderOnly("poster_32");
  VerifyPluginIsPlaceholderOnly("poster_16_64");
  VerifyPluginIsPlaceholderOnly("poster_64_16");

  VerifyPluginIsPlaceholderOnly("poster_obscured");

  EXPECT_TRUE(VerifySnapshot(FILE_PATH_LITERAL("poster_tests_expected.png")));

  // Test that posters can be unthrottled via click.
  SimulateClickAndAwaitMarkedEssential("plugin_src", gfx::Point(50, 50));
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, LargePostersNotThrottled) {
  // This test verifies that small posters are throttled, large posters are not,
  // and that large posters can whitelist origins for other plugins.
  LoadHTML("/large_posters_not_throttled.html");

  VerifyPluginIsPlaceholderOnly("poster_small");
  VerifyPluginMarkedEssential(GetActiveWebContents(),
                              "poster_whitelisted_origin");
  VerifyPluginMarkedEssential(GetActiveWebContents(),
                              "plugin_whitelisted_origin");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "poster_large");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, OriginWhitelisting) {
  LoadHTML("/origin_whitelisting.html");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin_small");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin_small_poster");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin_large");
}

// Flaky on almost all platforms: crbug.com/648827.
IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest,
                       DISABLED_LargeCrossOriginObscured) {
  LoadHTML("/large_cross_origin_obscured.html");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin");
  EXPECT_TRUE(VerifySnapshot(
      FILE_PATH_LITERAL("large_cross_origin_obscured_expected.png")));

  // Test that's unthrottled if it is unobscured.
  std::string script =
      "var container = window.document.getElementById('container');"
      "container.setAttribute('style', 'width: 400px; height: 400px;');";
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), script));
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, ExpandingSmallPlugin) {
  LoadPeripheralPlugin();
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin");

  std::string script = "window.document.getElementById('plugin').height = 400;";
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), script));
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, BackgroundTabPlugins) {
  content::WebContents* background_contents =
      LoadHTMLInBackgroundTab("/background_tab_plugins.html");

  EXPECT_FALSE(PluginLoaded(background_contents, "same_origin"));
  EXPECT_FALSE(PluginLoaded(background_contents, "small_cross_origin"));

  ActivateTab(background_contents);

  VerifyPluginMarkedEssential(background_contents, "same_origin");
  VerifyPluginIsThrottled(background_contents, "small_cross_origin");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, ZoomIndependent) {
  zoom::ZoomController::FromWebContents(GetActiveWebContents())
      ->SetZoomLevel(4.0);
  LoadHTML("/zoom_independent.html");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, BlockTinyPlugins) {
  LoadHTML("/block_tiny_plugins.html");

  VerifyPluginMarkedEssential(GetActiveWebContents(), "tiny_same_origin");
  VerifyPluginIsPlaceholderOnly("tiny_cross_origin_1");
  VerifyPluginIsPlaceholderOnly("tiny_cross_origin_2");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, BackgroundTabTinyPlugins) {
  content::WebContents* background_contents =
      LoadHTMLInBackgroundTab("/background_tab_tiny_plugins.html");
  EXPECT_FALSE(PluginLoaded(background_contents, "tiny"));

  ActivateTab(background_contents);
  VerifyPluginIsPlaceholderOnly("tiny");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, ExpandingTinyPlugins) {
  LoadHTML("/expanding_tiny_plugins.html");

  VerifyPluginIsPlaceholderOnly("expand_to_peripheral");
  VerifyPluginIsPlaceholderOnly("expand_to_essential");

  std::string script =
      "window.document.getElementById('expand_to_peripheral').height = 200;"
      "window.document.getElementById('expand_to_peripheral').width = 200;"
      "window.document.getElementById('expand_to_essential').height = 400;"
      "window.document.getElementById('expand_to_essential').width = 400;";
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), script));

  VerifyPluginIsThrottled(GetActiveWebContents(), "expand_to_peripheral");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "expand_to_essential");
}

// Separate test case with HTML By Default feature flag on.
class PluginPowerSaverPreferHtmlBrowserTest
    : public PluginPowerSaverBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    // Although this is redundant with the Field Trial testing configuration,
    // the official builders don't read that.
    feature_list.InitAndEnableFeature(features::kPreferHtmlOverPlugins);
  }

 private:
  base::test::ScopedFeatureList feature_list;
};

IN_PROC_BROWSER_TEST_F(PluginPowerSaverPreferHtmlBrowserTest,
                       ThrottlePluginsOnAllowContentSetting) {
  HostContentSettingsMap* content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());

  content_settings_map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                                 CONTENT_SETTING_ALLOW);
  LoadPeripheralPlugin();
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin");
}
