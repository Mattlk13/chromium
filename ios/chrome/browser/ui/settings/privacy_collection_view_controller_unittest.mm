// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy_collection_view_controller.h"

#include <memory>

#include "base/ios/ios_util.h"
#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/handoff/pref_names_ios.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_mock_factory.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/autofill/autofill_controller.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/experimental_flags.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/browser/prefs/browser_prefs.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#import "ios/chrome/browser/ui/contextual_search/touch_to_search_permissions_mediator.h"
#import "ios/chrome/browser/ui/settings/physical_web_collection_view_controller.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/web_capabilities.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

NSString* const kSpdyProxyEnabled = @"SpdyProxyEnabled";

class PrivacyCollectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  void SetUp() override {
    CollectionViewControllerTest::SetUp();
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.SetPrefService(CreatePrefService());
    chrome_browser_state_ = test_cbs_builder.Build();

    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    initialValueForSpdyProxyEnabled_.reset(
        [[defaults valueForKey:kSpdyProxyEnabled] copy]);
    [defaults setValue:@"Disabled" forKey:kSpdyProxyEnabled];
    CreateController();
  }

  void TearDown() override {
    if (initialValueForSpdyProxyEnabled_) {
      [[NSUserDefaults standardUserDefaults]
          setObject:initialValueForSpdyProxyEnabled_.get()
             forKey:kSpdyProxyEnabled];
    } else {
      [[NSUserDefaults standardUserDefaults]
          removeObjectForKey:kSpdyProxyEnabled];
    }
    CollectionViewControllerTest::TearDown();
  }

  // Makes a PrefService to be used by the test.
  std::unique_ptr<sync_preferences::PrefServiceSyncable> CreatePrefService() {
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable);
    RegisterBrowserStatePrefs(registry.get());
    sync_preferences::PrefServiceMockFactory factory;
    return factory.CreateSyncable(registry.get());
  }

  CollectionViewController* NewController() override {
    return [[PrivacyCollectionViewController alloc]
        initWithBrowserState:chrome_browser_state_.get()];
  }

  web::TestWebThreadBundle thread_bundle_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  base::scoped_nsobject<NSString> initialValueForSpdyProxyEnabled_;
};

// Tests PrivacyCollectionViewController is set up with all appropriate items
// and sections.
// TODO(http://crbug.com/677121): reenable this test.
TEST_F(PrivacyCollectionViewControllerTest, DISABLED_TestModel) {
  CheckController();
  EXPECT_EQ(4, NumberOfSections());

  int sectionIndex = 0;
  EXPECT_EQ(1, NumberOfItemsInSection(sectionIndex));
  CheckSectionHeaderWithId(IDS_IOS_OPTIONS_CONTINUITY_LABEL, sectionIndex);
  NSString* handoffSubtitle = chrome_browser_state_->GetPrefs()->GetBoolean(
                                  prefs::kIosHandoffToOtherDevices)
                                  ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                                  : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  CheckTextCellTitleAndSubtitle(
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ENABLE_HANDOFF_TO_OTHER_DEVICES),
      handoffSubtitle, sectionIndex, 0);

  ++sectionIndex;
  NSInteger expectedRows = 1;
#if defined(GOOGLE_CHROME_BUILD)
  expectedRows++;
#endif
  if ([TouchToSearchPermissionsMediator isTouchToSearchAvailableOnDevice])
    expectedRows++;
  if (web::IsDoNotTrackSupported())
    expectedRows++;
  if (experimental_flags::IsPhysicalWebEnabled())
    expectedRows++;
  EXPECT_EQ(expectedRows, NumberOfItemsInSection(sectionIndex));

  CheckSectionHeaderWithId(IDS_IOS_OPTIONS_WEB_SERVICES_LABEL, sectionIndex);
  base::scoped_nsobject<TouchToSearchPermissionsMediator>
      touchToSearchPermissions([[TouchToSearchPermissionsMediator alloc]
          initWithBrowserState:chrome_browser_state_.get()]);
  NSString* contextualSearchSubtitle =
      ([touchToSearchPermissions preferenceState] == TouchToSearch::DISABLED)
          ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
          : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  int row = 0;

  CheckSwitchCellStateAndTitleWithId(
      YES, IDS_IOS_OPTIONS_SEARCH_URL_SUGGESTIONS, sectionIndex, row++);

  if ([TouchToSearchPermissionsMediator isTouchToSearchAvailableOnDevice]) {
    CheckTextCellTitleAndSubtitle(
        l10n_util::GetNSString(IDS_IOS_CONTEXTUAL_SEARCH_TITLE),
        contextualSearchSubtitle, sectionIndex, row++);
  }
#if defined(GOOGLE_CHROME_BUILD)
  CheckTextCellTitleWithId(IDS_IOS_OPTIONS_SEND_USAGE_DATA, sectionIndex,
                           row++);
#endif
  if (web::IsDoNotTrackSupported()) {
    NSString* doNotTrackSubtitle =
        chrome_browser_state_->GetPrefs()->GetBoolean(prefs::kEnableDoNotTrack)
            ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
            : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    CheckTextCellTitleAndSubtitle(
        l10n_util::GetNSString(IDS_IOS_OPTIONS_DO_NOT_TRACK_MOBILE),
        doNotTrackSubtitle, sectionIndex, row++);
  }
  if (experimental_flags::IsPhysicalWebEnabled()) {
    NSInteger physicalWebState =
        GetApplicationContext()->GetLocalState()->GetInteger(
            prefs::kIosPhysicalWebEnabled);
    BOOL physicalWebEnabled = [PhysicalWebCollectionViewController
        shouldEnableForPreferenceState:physicalWebState];
    NSString* physicalWebSubtitle =
        physicalWebEnabled ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                           : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    CheckTextCellTitleAndSubtitle(
        l10n_util::GetNSString(IDS_IOS_OPTIONS_ENABLE_PHYSICAL_WEB),
        physicalWebSubtitle, sectionIndex, row++);
  }

  sectionIndex++;
  EXPECT_EQ(1, NumberOfItemsInSection(sectionIndex));
  CheckSectionFooterWithId(IDS_IOS_OPTIONS_PRIVACY_FOOTER, sectionIndex);

  sectionIndex++;
  EXPECT_EQ(1, NumberOfItemsInSection(sectionIndex));
  CheckTextCellTitle(l10n_util::GetNSString(IDS_IOS_CLEAR_BROWSING_DATA_TITLE),
                     sectionIndex, 0);
}

}  // namespace
