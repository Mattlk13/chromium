// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "build/build_config.h"
#include "chrome/browser/media/media_browsertest.h"
#include "chrome/browser/media/test_license_server.h"
#include "chrome/browser/media/wv_test_license_server_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/test_launcher_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "media/base/media_switches.h"
#include "ppapi/features/features.h"
#include "testing/gtest/include/gtest/gtest-spi.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#endif

#if BUILDFLAG(ENABLE_PEPPER_CDMS)
#include "chrome/browser/media/pepper_cdm_test_constants.h"
#include "chrome/browser/media/pepper_cdm_test_helper.h"
#include "media/base/media_switches.h"
#endif

#include "widevine_cdm_version.h"  //  In SHARED_INTERMEDIATE_DIR.

// Available key systems.
const char kClearKeyKeySystem[] = "org.w3.clearkey";
const char kExternalClearKeyKeySystem[] = "org.chromium.externalclearkey";

// Variants of External Clear Key key system to test different scenarios.
// To add a new variant, make sure you also update:
// - media/test/data/eme_player_js/globals.js
// - AddExternalClearKey() in chrome_key_systems.cc
// - CreateCdmInstance() in clear_key_cdm.cc
const char kExternalClearKeyRenewalKeySystem[] =
    "org.chromium.externalclearkey.renewal";
const char kExternalClearKeyFileIOTestKeySystem[] =
    "org.chromium.externalclearkey.fileiotest";
const char kExternalClearKeyInitializeFailKeySystem[] =
    "org.chromium.externalclearkey.initializefail";
const char kExternalClearKeyOutputProtectionTestKeySystem[] =
    "org.chromium.externalclearkey.outputprotectiontest";
const char kExternalClearKeyPlatformVerificationTestKeySystem[] =
    "org.chromium.externalclearkey.platformverificationtest";
const char kExternalClearKeyCrashKeySystem[] =
    "org.chromium.externalclearkey.crash";

// Supported media types.
const char kWebMVorbisAudioOnly[] = "audio/webm; codecs=\"vorbis\"";
const char kWebMOpusAudioOnly[] = "audio/webm; codecs=\"opus\"";
const char kWebMVP8VideoOnly[] = "video/webm; codecs=\"vp8\"";
const char kWebMVorbisAudioVP8Video[] = "video/webm; codecs=\"vorbis, vp8\"";
const char kWebMOpusAudioVP9Video[] = "video/webm; codecs=\"opus, vp9\"";
const char kWebMVP9VideoOnly[] = "video/webm; codecs=\"vp9\"";
#if defined(USE_PROPRIETARY_CODECS)
const char kMP4AudioOnly[] = "audio/mp4; codecs=\"mp4a.40.2\"";
const char kMP4VideoOnly[] = "video/mp4; codecs=\"avc1.4D000C\"";
const char kMP4VideoVp9Only[] =
    "video/mp4; codecs=\"vp09.00.01.08.02.01.01.00\"";
#endif  // defined(USE_PROPRIETARY_CODECS)

// Sessions to load.
const char kNoSessionToLoad[] = "";
const char kLoadableSession[] = "LoadableSession";
const char kUnknownSession[] = "UnknownSession";

// EME-specific test results and errors.
const char kUnitTestSuccess[] = "UNIT_TEST_SUCCESS";
const char kEmeNotSupportedError[] = "NOTSUPPORTEDERROR";
const char kEmeGenerateRequestFailed[] = "EME_GENERATEREQUEST_FAILED";
const char kEmeSessionNotFound[] = "EME_SESSION_NOT_FOUND";
const char kEmeLoadFailed[] = "EME_LOAD_FAILED";
const char kEmeUpdateFailed[] = "EME_UPDATE_FAILED";
const char kEmeErrorEvent[] = "EME_ERROR_EVENT";
const char kEmeMessageUnexpectedType[] = "EME_MESSAGE_UNEXPECTED_TYPE";
const char kEmeRenewalMissingHeader[] = "EME_RENEWAL_MISSING_HEADER";
const char kEmeSessionClosedAndError[] = "EME_SESSION_CLOSED_AND_ERROR";

const char kDefaultEmePlayer[] = "eme_player.html";

// The type of video src used to load media.
enum SrcType {
  SRC,
  MSE
};

// Whether the video should be played once or twice.
enum class PlayTwice { NO, YES };

// Format of a container when testing different streams.
enum class EncryptedContainer {
  CLEAR_WEBM,
  CLEAR_MP4,
  ENCRYPTED_WEBM,
  ENCRYPTED_MP4
};

// Base class for encrypted media tests.
class EncryptedMediaTestBase : public MediaBrowserTest {
 public:
  bool IsExternalClearKey(const std::string& key_system) {
    if (key_system == kExternalClearKeyKeySystem)
      return true;
    std::string prefix = std::string(kExternalClearKeyKeySystem) + '.';
    return key_system.substr(0, prefix.size()) == prefix;
  }

#if defined(WIDEVINE_CDM_AVAILABLE)
  bool IsWidevine(const std::string& key_system) {
    return key_system == kWidevineKeySystem;
  }
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

  void RunEncryptedMediaTestPage(
      const std::string& html_page,
      const std::string& key_system,
      base::StringPairs& query_params,
      const std::string& expected_title) {
    base::StringPairs new_query_params = query_params;
    StartLicenseServerIfNeeded(key_system, &new_query_params);
    RunMediaTestPage(html_page, new_query_params, expected_title, true);
  }

  // Tests |html_page| using |media_file| (with |media_type|) and |key_system|.
  // When |session_to_load| is not empty, the test will try to load
  // |session_to_load| with stored keys, instead of creating a new session
  // and trying to update it with licenses.
  // When |force_invalid_response| is true, the test will provide invalid
  // responses, which should trigger errors.
  // TODO(xhwang): Find an easier way to pass multiple configuration test
  // options.
  void RunEncryptedMediaTest(const std::string& html_page,
                             const std::string& media_file,
                             const std::string& media_type,
                             const std::string& key_system,
                             SrcType src_type,
                             const std::string& session_to_load,
                             bool force_invalid_response,
                             PlayTwice play_twice,
                             const std::string& expected_title) {
    base::StringPairs query_params;
    query_params.push_back(std::make_pair("mediaFile", media_file));
    query_params.push_back(std::make_pair("mediaType", media_type));
    query_params.push_back(std::make_pair("keySystem", key_system));
    if (src_type == MSE)
      query_params.push_back(std::make_pair("useMSE", "1"));
    if (force_invalid_response)
      query_params.push_back(std::make_pair("forceInvalidResponse", "1"));
    if (!session_to_load.empty())
      query_params.push_back(std::make_pair("sessionToLoad", session_to_load));
    if (play_twice == PlayTwice::YES)
      query_params.push_back(std::make_pair("playTwice", "1"));
    RunEncryptedMediaTestPage(html_page, key_system, query_params,
                              expected_title);
  }

  void RunSimpleEncryptedMediaTest(const std::string& media_file,
                                   const std::string& media_type,
                                   const std::string& key_system,
                                   SrcType src_type) {
    std::string expected_title = kEnded;
    if (!IsPlayBackPossible(key_system)) {
      expected_title = kEmeUpdateFailed;
    }

    RunEncryptedMediaTest(kDefaultEmePlayer, media_file, media_type, key_system,
                          src_type, kNoSessionToLoad, false, PlayTwice::NO,
                          expected_title);
    // Check KeyMessage received for all key systems.
    bool receivedKeyMessage = false;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send("
        "document.querySelector('video').receivedKeyMessage);",
        &receivedKeyMessage));
    EXPECT_TRUE(receivedKeyMessage);
  }

  // Starts a license server if available for the |key_system| and adds a
  // 'licenseServerURL' query parameter to |query_params|.
  void StartLicenseServerIfNeeded(const std::string& key_system,
                                  base::StringPairs* query_params) {
    std::unique_ptr<TestLicenseServerConfig> config =
        GetServerConfig(key_system);
    if (!config)
      return;
    license_server_.reset(new TestLicenseServer(std::move(config)));
    EXPECT_TRUE(license_server_->Start());
    query_params->push_back(
        std::make_pair("licenseServerURL", license_server_->GetServerURL()));
  }

  bool IsPlayBackPossible(const std::string& key_system) {
#if defined(WIDEVINE_CDM_AVAILABLE)
    if (IsWidevine(key_system) && !GetServerConfig(key_system))
      return false;
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
    return true;
  }

  std::unique_ptr<TestLicenseServerConfig> GetServerConfig(
      const std::string& key_system) {
#if defined(WIDEVINE_CDM_AVAILABLE)
    if (IsWidevine(key_system)) {
      std::unique_ptr<TestLicenseServerConfig> config(
          new WVTestLicenseServerConfig);
      if (config->IsPlatformSupported())
        return config;
    }
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
    return nullptr;
  }

 protected:
  std::unique_ptr<TestLicenseServer> license_server_;

  // We want to fail quickly when a test fails because an error is encountered.
  void AddWaitForTitles(content::TitleWatcher* title_watcher) override {
    MediaBrowserTest::AddWaitForTitles(title_watcher);
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeNotSupportedError));
    title_watcher->AlsoWaitForTitle(
        base::ASCIIToUTF16(kEmeGenerateRequestFailed));
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeSessionNotFound));
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeLoadFailed));
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeUpdateFailed));
    title_watcher->AlsoWaitForTitle(base::ASCIIToUTF16(kEmeErrorEvent));
    title_watcher->AlsoWaitForTitle(
        base::ASCIIToUTF16(kEmeMessageUnexpectedType));
    title_watcher->AlsoWaitForTitle(
        base::ASCIIToUTF16(kEmeRenewalMissingHeader));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kDisableGestureRequirementForMediaPlayback);
    command_line->AppendSwitch(switches::kEnableVp9InMp4);
  }

#if BUILDFLAG(ENABLE_PEPPER_CDMS)
  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    base::CommandLine default_command_line(base::CommandLine::NO_PROGRAM);
    InProcessBrowserTest::SetUpDefaultCommandLine(&default_command_line);
    test_launcher_utils::RemoveCommandLineSwitch(
        default_command_line, switches::kDisableComponentUpdate, command_line);
  }
#endif  // BUILDFLAG(ENABLE_PEPPER_CDMS)

  void SetUpCommandLineForKeySystem(const std::string& key_system,
                                    base::CommandLine* command_line) {
    if (GetServerConfig(key_system))
      // Since the web and license servers listen on different ports, we need to
      // disable web-security to send license requests to the license server.
      // TODO(shadi): Add port forwarding to the test web server configuration.
      command_line->AppendSwitch(switches::kDisableWebSecurity);

#if BUILDFLAG(ENABLE_PEPPER_CDMS)
    if (IsExternalClearKey(key_system)) {
      RegisterPepperCdm(command_line, kClearKeyCdmBaseDirectory,
                        kClearKeyCdmAdapterFileName, kClearKeyCdmDisplayName,
                        kClearKeyCdmPepperMimeType);
      command_line->AppendSwitchASCII(switches::kEnableFeatures,
                                      media::kExternalClearKeyForTesting.name);
    }
#endif  // BUILDFLAG(ENABLE_PEPPER_CDMS)
  }
};

#if BUILDFLAG(ENABLE_PEPPER_CDMS)
// Tests encrypted media playback using ExternalClearKey key system in
// decrypt-and-decode mode.
class ECKEncryptedMediaTest : public EncryptedMediaTestBase {
 public:
  // We use special |key_system| names to do non-playback related tests, e.g.
  // kExternalClearKeyFileIOTestKeySystem is used to test file IO.
  void TestNonPlaybackCases(const std::string& key_system,
                            const std::string& expected_title) {
    // Since we do not test playback, arbitrarily choose a test file and source
    // type.
    RunEncryptedMediaTest(kDefaultEmePlayer, "bear-a_enc-a.webm",
                          kWebMVorbisAudioOnly, key_system, SRC,
                          kNoSessionToLoad, false, PlayTwice::NO,
                          expected_title);
  }

  void TestPlaybackCase(const std::string& key_system,
                        const std::string& session_to_load,
                        const std::string& expected_title) {
    RunEncryptedMediaTest(kDefaultEmePlayer, "bear-320x240-v_enc-v.webm",
                          kWebMVP8VideoOnly, key_system, SRC, session_to_load,
                          false, PlayTwice::NO, expected_title);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    SetUpCommandLineForKeySystem(kExternalClearKeyKeySystem, command_line);
  }
};

#if defined(WIDEVINE_CDM_AVAILABLE)
// Tests encrypted media playback using Widevine key system.
class WVEncryptedMediaTest : public EncryptedMediaTestBase {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    SetUpCommandLineForKeySystem(kWidevineKeySystem, command_line);
  }
};

#endif  // defined(WIDEVINE_CDM_AVAILABLE)
#endif  // BUILDFLAG(ENABLE_PEPPER_CDMS)

// Tests encrypted media playback with a combination of parameters:
// - char*: Key system name.
// - SrcType: Use MSE or SRC.
//
// Note: Only parameterized (*_P) tests can be used. Non-parameterized (*_F)
// tests will crash at GetParam(). To add non-parameterized tests, use
// EncryptedMediaTestBase or one of its subclasses (e.g. WVEncryptedMediaTest).
class EncryptedMediaTest : public EncryptedMediaTestBase,
                           public testing::WithParamInterface<
                               std::tr1::tuple<const char*, SrcType>> {
 public:
  std::string CurrentKeySystem() {
    return std::tr1::get<0>(GetParam());
  }

  SrcType CurrentSourceType() {
    return std::tr1::get<1>(GetParam());
  }

  void TestSimplePlayback(const std::string& encrypted_media,
                          const std::string& media_type) {
    RunSimpleEncryptedMediaTest(encrypted_media, media_type, CurrentKeySystem(),
                                CurrentSourceType());
  }

  void TestMultiplePlayback(const std::string& encrypted_media,
                            const std::string& media_type) {
    DCHECK(IsPlayBackPossible(CurrentKeySystem()));
    RunEncryptedMediaTest(kDefaultEmePlayer, encrypted_media, media_type,
                          CurrentKeySystem(), CurrentSourceType(),
                          kNoSessionToLoad, false, PlayTwice::YES, kEnded);
  }

  void RunInvalidResponseTest() {
    RunEncryptedMediaTest(kDefaultEmePlayer, "bear-320x240-av_enc-av.webm",
                          kWebMVorbisAudioVP8Video, CurrentKeySystem(),
                          CurrentSourceType(), kNoSessionToLoad, true,
                          PlayTwice::NO, kEmeUpdateFailed);
  }

  void TestFrameSizeChange() {
    RunEncryptedMediaTest(
        "encrypted_frame_size_change.html", "frame_size_change-av_enc-v.webm",
        kWebMVorbisAudioVP8Video, CurrentKeySystem(), CurrentSourceType(),
        kNoSessionToLoad, false, PlayTwice::NO, kEnded);
  }

  void TestConfigChange() {
    base::StringPairs query_params;
    query_params.push_back(std::make_pair("keySystem", CurrentKeySystem()));
    query_params.push_back(std::make_pair("runEncrypted", "1"));
    RunEncryptedMediaTestPage("mse_config_change.html",
                              CurrentKeySystem(),
                              query_params,
                              kEnded);
  }

  std::string ConvertContainerFormat(EncryptedContainer format) {
    switch (format) {
      case EncryptedContainer::CLEAR_MP4:
        return "CLEAR_MP4";
      case EncryptedContainer::CLEAR_WEBM:
        return "CLEAR_WEBM";
      case EncryptedContainer::ENCRYPTED_MP4:
        return "ENCRYPTED_MP4";
      case EncryptedContainer::ENCRYPTED_WEBM:
        return "ENCRYPTED_WEBM";
    }
    NOTREACHED();
    return "UNKNOWN";
  }

  void TestDifferentContainers(EncryptedContainer video_format,
                               EncryptedContainer audio_format) {
    base::StringPairs query_params;
    query_params.push_back(std::make_pair("keySystem", CurrentKeySystem()));
    query_params.push_back(std::make_pair("runEncrypted", "1"));
    query_params.push_back(
        std::make_pair("videoFormat", ConvertContainerFormat(video_format)));
    query_params.push_back(
        std::make_pair("audioFormat", ConvertContainerFormat(audio_format)));
    RunEncryptedMediaTestPage("mse_different_containers.html",
                              CurrentKeySystem(), query_params, kEnded);
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    EncryptedMediaTestBase::SetUpCommandLine(command_line);
    SetUpCommandLineForKeySystem(CurrentKeySystem(), command_line);
  }
};

using ::testing::Combine;
using ::testing::Values;

#if !defined(OS_ANDROID)
INSTANTIATE_TEST_CASE_P(SRC_ClearKey,
                        EncryptedMediaTest,
                        Combine(Values(kClearKeyKeySystem), Values(SRC)));
#endif  // !defined(OS_ANDROID)

INSTANTIATE_TEST_CASE_P(MSE_ClearKey,
                        EncryptedMediaTest,
                        Combine(Values(kClearKeyKeySystem), Values(MSE)));

// External Clear Key is currently only used on platforms that use Pepper CDMs.
#if BUILDFLAG(ENABLE_PEPPER_CDMS)
INSTANTIATE_TEST_CASE_P(SRC_ExternalClearKey,
                        EncryptedMediaTest,
                        Combine(Values(kExternalClearKeyKeySystem),
                                Values(SRC)));

INSTANTIATE_TEST_CASE_P(MSE_ExternalClearKey,
                        EncryptedMediaTest,
                        Combine(Values(kExternalClearKeyKeySystem),
                                Values(MSE)));

const char kExternalClearKeyDecryptOnlyKeySystem[] =
    "org.chromium.externalclearkey.decryptonly";

// To reduce test time, only run ExternalClearKeyDecryptOnly with MSE.
INSTANTIATE_TEST_CASE_P(MSE_ExternalClearKeyDecryptOnly,
                        EncryptedMediaTest,
                        Combine(Values(kExternalClearKeyDecryptOnlyKeySystem),
                                Values(MSE)));
#endif  // BUILDFLAG(ENABLE_PEPPER_CDMS)

#if defined(WIDEVINE_CDM_AVAILABLE)
#if !defined(OS_CHROMEOS)
INSTANTIATE_TEST_CASE_P(MSE_Widevine,
                        EncryptedMediaTest,
                        Combine(Values(kWidevineKeySystem), Values(MSE)));
#endif  // !defined(OS_CHROMEOS)
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_WebM) {
  TestSimplePlayback("bear-a_enc-a.webm", kWebMVorbisAudioOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioClearVideo_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-a.webm", kWebMVorbisAudioVP8Video);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoAudio_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-av.webm", kWebMVorbisAudioVP8Video);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_WebM) {
  TestSimplePlayback("bear-320x240-v_enc-v.webm", kWebMVP8VideoOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoClearAudio_WebM) {
  TestSimplePlayback("bear-320x240-av_enc-v.webm", kWebMVorbisAudioVP8Video);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VP9Video_WebM_Fullsample) {
  TestSimplePlayback("bear-320x240-v-vp9_fullsample_enc-v.webm",
                     kWebMVP9VideoOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VP9Video_WebM_Subsample) {
  TestSimplePlayback("bear-320x240-v-vp9_subsample_enc-v.webm",
                     kWebMVP9VideoOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_WebM_Opus) {
  TestSimplePlayback("bear-320x240-opus-a_enc-a.webm", kWebMOpusAudioOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoAudio_WebM_Opus) {
  TestSimplePlayback("bear-320x240-opus-av_enc-av.webm",
                     kWebMOpusAudioVP9Video);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoClearAudio_WebM_Opus) {
  TestSimplePlayback("bear-320x240-opus-av_enc-v.webm", kWebMOpusAudioVP9Video);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_Multiple_VideoAudio_WebM) {
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - Playback_Multiple test requires playback.";
    return;
  }
  TestMultiplePlayback("bear-320x240-av_enc-av.webm", kWebMVorbisAudioVP8Video);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, InvalidResponseKeyError) {
  RunInvalidResponseTest();
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, ConfigChangeVideo) {
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - ConfigChange test requires video playback.";
    return;
  }
  TestConfigChange();
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, FrameSizeChangeVideo) {
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - FrameSizeChange test requires video playback.";
    return;
  }
  TestFrameSizeChange();
}

#if defined(USE_PROPRIETARY_CODECS)
// Crashes on Mac only.  http://crbug.com/621857
#if defined(OS_MACOSX)
#define MAYBE_Playback_VideoOnly_MP4 DISABLED_Playback_VideoOnly_MP4
#else
#define MAYBE_Playback_VideoOnly_MP4 Playback_VideoOnly_MP4
#endif
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, MAYBE_Playback_VideoOnly_MP4) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestSimplePlayback("bear-640x360-v_frag-cenc.mp4", kMP4VideoOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_AudioOnly_MP4) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestSimplePlayback("bear-640x360-a_frag-cenc.mp4", kMP4AudioOnly);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, Playback_VideoOnly_MP4_VP9) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  TestSimplePlayback("bear-320x240-v_frag-vp9-cenc.mp4", kMP4VideoVp9Only);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       Playback_EncryptedVideo_MP4_ClearAudio_WEBM) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - Test requires video playback.";
    return;
  }
  TestDifferentContainers(EncryptedContainer::ENCRYPTED_MP4,
                          EncryptedContainer::CLEAR_WEBM);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       Playback_ClearVideo_WEBM_EncryptedAudio_MP4) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - Test requires video playback.";
    return;
  }
  TestDifferentContainers(EncryptedContainer::CLEAR_WEBM,
                          EncryptedContainer::ENCRYPTED_MP4);
}

IN_PROC_BROWSER_TEST_P(EncryptedMediaTest,
                       Playback_EncryptedVideo_WEBM_EncryptedAudio_MP4) {
  // MP4 without MSE is not support yet, http://crbug.com/170793.
  if (CurrentSourceType() != MSE) {
    DVLOG(0) << "Skipping test; Can only play MP4 encrypted streams by MSE.";
    return;
  }
  if (!IsPlayBackPossible(CurrentKeySystem())) {
    DVLOG(0) << "Skipping test - Test requires video playback.";
    return;
  }
  TestDifferentContainers(EncryptedContainer::ENCRYPTED_WEBM,
                          EncryptedContainer::ENCRYPTED_MP4);
}
#endif  // defined(USE_PROPRIETARY_CODECS)

#if defined(WIDEVINE_CDM_AVAILABLE)
// The parent key system cannot be used when creating MediaKeys.
IN_PROC_BROWSER_TEST_F(WVEncryptedMediaTest, ParentThrowsException) {
  RunEncryptedMediaTest(kDefaultEmePlayer, "bear-a_enc-a.webm",
                        kWebMVorbisAudioOnly, "com.widevine", MSE,
                        kNoSessionToLoad, false, PlayTwice::NO,
                        kEmeNotSupportedError);
}
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

#if BUILDFLAG(ENABLE_PEPPER_CDMS)
IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, InitializeCDMFail) {
  TestNonPlaybackCases(kExternalClearKeyInitializeFailKeySystem,
                       kEmeNotSupportedError);
}

// When CDM crashes, we should still get a decode error and all sessions should
// be closed.
IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, CDMCrashDuringDecode) {
  IgnorePluginCrash();
  TestNonPlaybackCases(kExternalClearKeyCrashKeySystem,
                       kEmeSessionClosedAndError);
}

// Testing that the media browser test does fail on plugin crash.
IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, CDMExpectedCrash) {
  // Plugin crash is not ignored by default, the test is expected to fail.
  EXPECT_NONFATAL_FAILURE(TestNonPlaybackCases(kExternalClearKeyCrashKeySystem,
                                               kEmeSessionClosedAndError),
                          "Failing test due to plugin crash.");
}

IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, FileIOTest) {
  TestNonPlaybackCases(kExternalClearKeyFileIOTestKeySystem, kUnitTestSuccess);
}

// TODO(xhwang): Investigate how to fake capturing activities to test the
// network link detection logic in OutputProtectionProxy.
IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, OutputProtectionTest) {
  TestNonPlaybackCases(kExternalClearKeyOutputProtectionTestKeySystem,
                       kUnitTestSuccess);
}

IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, PlatformVerificationTest) {
  TestNonPlaybackCases(kExternalClearKeyPlatformVerificationTestKeySystem,
                       kUnitTestSuccess);
}

IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, Renewal) {
  TestPlaybackCase(kExternalClearKeyRenewalKeySystem, kNoSessionToLoad, kEnded);
}

IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, LoadLoadableSession) {
  TestPlaybackCase(kExternalClearKeyKeySystem, kLoadableSession, kEnded);
}

IN_PROC_BROWSER_TEST_F(ECKEncryptedMediaTest, LoadUnknownSession) {
  TestPlaybackCase(kExternalClearKeyKeySystem, kUnknownSession,
                   kEmeSessionNotFound);
}

#endif  // BUILDFLAG(ENABLE_PEPPER_CDMS)
