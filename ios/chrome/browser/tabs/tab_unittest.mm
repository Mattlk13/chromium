// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include <memory>

#include "base/callback.h"
#include "base/ios/block_types.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/chrome_url_util.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_private.h"
#import "ios/chrome/browser/ui/open_in_controller.h"
#import "ios/chrome/browser/ui/open_in_controller_testing.h"
#import "ios/chrome/browser/web/external_app_launcher.h"
#include "ios/chrome/test/block_cleanup_test.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_provider.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/fake_native_app_metadata.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/fake_native_app_whitelist_manager.h"
#include "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/navigation_item.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/mac/url_conversions.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using web::WebStateImpl;

static const char kNewTabUrl[] = "chrome://newtab/";
static NSString* const kNewTabTitle = @"New Tab";
static const char kGoogleUserUrl[] = "http://google.com";
static const char kGoogleRedirectUrl[] = "http://www.google.fr/";
static NSString* const kGoogleTitle = @"Google";
static const char kOtherUserUrl[] = "http://other.com";
static const char kOtherRedirectUrl[] = "http://other.fr/";
static NSString* const kOtherTitle = @"Other";
const char kContentDispositionWithFilename[] =
    "attachment; filename=\"suggested_filename.pdf\"";
const char kContentDispositionWithoutFilename[] =
    "attachment; parameter=parameter_value";
const char kInvalidFilenameUrl[] = "http://www.hostname.com/";
const char kValidFilenameUrl[] = "http://www.hostname.com/filename.pdf";

@interface ArrayTabModel : TabModel {
 @private
  base::scoped_nsobject<NSMutableArray> tabsForTesting_;
}
@end

@implementation ArrayTabModel
- (id)init {
  if ((self = [super init]))
    tabsForTesting_.reset([[NSMutableArray alloc] initWithCapacity:1]);
  return self;
}

- (void)addTabForTesting:(Tab*)tab {
  [tabsForTesting_ addObject:tab];
}

- (NSUInteger)indexOfTab:(Tab*)tab {
  return [tabsForTesting_ indexOfObject:tab];
}

- (NSUInteger)count {
  return [tabsForTesting_ count];
}

- (void)closeTabAtIndex:(NSUInteger)index {
  [tabsForTesting_ removeObjectAtIndex:index];
}

- (void)didCloseTab:(Tab*)closedTab {
}
@end

@interface ExternalAppLauncherMock : OCMockComplexTypeHelper
@end

@implementation ExternalAppLauncherMock
typedef BOOL (^openURLBlockType)(const GURL&, BOOL);

- (BOOL)openURL:(const GURL&)url linkClicked:(BOOL)linkClicked {
  return static_cast<openURLBlockType>([self blockForSelector:_cmd])(
      url, linkClicked);
}
@end

namespace {

const web::LoadPhase kLoadRequested = web::LOAD_REQUESTED;
const web::LoadPhase kPageLoading = web::PAGE_LOADING;
const web::LoadPhase kPageLoaded = web::PAGE_LOADED;

// Observer of a QueryHistory request.
class HistoryQueryResultsObserver
    : public base::RefCountedThreadSafe<HistoryQueryResultsObserver> {
 public:
  HistoryQueryResultsObserver(base::RunLoop* run_loop) : run_loop_(run_loop) {}

  // Stores |results| and stops the current message loop.
  void ProcessResults(history::QueryResults* results) {
    results_.Swap(results);
    run_loop_->QuitWhenIdle();
  }
  history::QueryResults* results() { return &results_; }

 protected:
  friend base::RefCountedThreadSafe<HistoryQueryResultsObserver>;
  virtual ~HistoryQueryResultsObserver();

 private:
  history::QueryResults results_;
  base::RunLoop* run_loop_;
};

HistoryQueryResultsObserver::~HistoryQueryResultsObserver() {}

class FakeChromeBrowserProvider : public ios::TestChromeBrowserProvider {
 public:
  FakeChromeBrowserProvider(id<NativeAppMetadata> metadata) {
    FakeNativeAppWhitelistManager* fakeManager =
        [[[FakeNativeAppWhitelistManager alloc] init] autorelease];
    fakeManager.metadata = metadata;
    manager_.reset([fakeManager retain]);
  }
  ~FakeChromeBrowserProvider() override {}

  id<NativeAppWhitelistManager> GetNativeAppWhitelistManager() const override {
    return manager_;
  }

 private:
  base::scoped_nsprotocol<id<NativeAppWhitelistManager>> manager_;
};

class TabTest : public BlockCleanupTest {
 public:
  TabTest() : thread_bundle_(web::TestWebThreadBundle::REAL_FILE_THREAD) {}

  void SetUp() override {
    BlockCleanupTest::SetUp();

    [[ChromeAppConstants sharedInstance]
        setCallbackSchemeForTesting:@"chromium"];

    // Set up the testing profiles.
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    chrome_browser_state_->CreateBookmarkModel(false);
    bookmarks::test::WaitForBookmarkModelToLoad(
        ios::BookmarkModelFactory::GetForBrowserState(
            chrome_browser_state_.get()));
    ASSERT_TRUE(chrome_browser_state_->CreateHistoryService(true));
    history_service_ = ios::HistoryServiceFactory::GetForBrowserState(
        chrome_browser_state_.get(), ServiceAccessType::EXPLICIT_ACCESS);

    ios::ChromeBrowserState* browser_state = chrome_browser_state_.get();
    if (UseOffTheRecordBrowserState())
      browser_state = browser_state->GetOffTheRecordChromeBrowserState();

    mock_web_controller_ =
        [OCMockObject niceMockForClass:[CRWWebController class]];
    std::unique_ptr<WebStateImpl> web_state_impl;
    web_state_impl.reset(new WebStateImpl(browser_state));
    web_state_impl->SetWebController(mock_web_controller_);
    web_state_impl->GetNavigationManagerImpl().InitializeSession(
        @"window1", @"opener", NO, 0);
    WebStateImpl* web_state = web_state_impl.get();
    [[[(OCMockObject*)mock_web_controller_ stub]
        andReturnValue:OCMOCK_VALUE(web_state)] webStateImpl];
    web_controller_view_.reset([[UIView alloc] init]);
    [[[(OCMockObject*)mock_web_controller_ stub]
        andReturn:web_controller_view_.get()] view];
    tab_.reset([[Tab alloc] initWithWindowName:nil
                                        opener:nullptr
                                   openedByDOM:NO
                                         model:nil
                                  browserState:browser_state]);
    web::NavigationManager::WebLoadParams params(GURL("chrome://version/"));
    [[tab_ webController] loadWithParams:params];
    [tab_ replaceWebStateImpl:std::move(web_state_impl)];

    // There should be no entries in the history at this point.
    history::QueryResults results;
    QueryAllHistory(&results);
    EXPECT_EQ(0UL, results.size());
    mock_external_app_launcher_.reset([[ExternalAppLauncherMock alloc]
        initWithRepresentedObject:
            [OCMockObject mockForClass:[ExternalAppLauncher class]]]);
    [tab_ replaceExternalAppLauncher:mock_external_app_launcher_];
  }

  void TearDown() override {
    [tab_ close];

    BlockCleanupTest::TearDown();
  }

  void BrowseTo(const GURL& userUrl, const GURL& redirectUrl, NSString* title) {
    [[[(id)mock_web_controller_ expect]
        andReturnValue:OCMOCK_VALUE(kLoadRequested)] loadPhase];
    web::Referrer empty_referrer;
    [tab_ webWillAddPendingURL:userUrl transition:ui::PAGE_TRANSITION_TYPED];
    [tab_ webDidAddPendingURL];
    [tab_ webWillAddPendingURL:redirectUrl
                    transition:ui::PAGE_TRANSITION_CLIENT_REDIRECT];
    [[tab_ navigationManager]->GetSessionController()
          addPendingEntry:redirectUrl
                 referrer:empty_referrer
               transition:ui::PAGE_TRANSITION_CLIENT_REDIRECT
        rendererInitiated:YES];
    [tab_ webDidAddPendingURL];
    [[[(id)mock_web_controller_ expect]
        andReturnValue:OCMOCK_VALUE(kPageLoading)] loadPhase];
    [[tab_ navigationManager]->GetSessionController() commitPendingEntry];
    [[tab_ webController] webStateImpl]->OnNavigationCommitted(redirectUrl);
    [tab_ webDidStartLoadingURL:redirectUrl shouldUpdateHistory:YES];
    [tab_ webController:mock_web_controller_ titleDidChange:title];
    [[[(id)mock_web_controller_ expect]
        andReturnValue:OCMOCK_VALUE(kPageLoaded)] loadPhase];
    [[tab_ webController] webStateImpl]->OnPageLoaded(redirectUrl, true);
    [tab_ webDidFinishWithURL:redirectUrl loadSuccess:YES];
  }

  void BrowseToNewTab() {
    const GURL url(kNewTabUrl);
    // TODO(crbug.com/661992): This will not work with a mock CRWWebController.
    // The only test that uses it is currently disabled.
    web::NavigationManager::WebLoadParams params(url);
    params.transition_type = ui::PAGE_TRANSITION_TYPED;
    [[tab_ webController] loadWithParams:params];
    [[[(id)mock_web_controller_ expect]
        andReturnValue:OCMOCK_VALUE(kPageLoading)] loadPhase];
    [tab_ webDidStartLoadingURL:url shouldUpdateHistory:YES];
    [[[(id)mock_web_controller_ expect]
        andReturnValue:OCMOCK_VALUE(kPageLoaded)] loadPhase];
    [tab_ webDidFinishWithURL:url loadSuccess:YES];
    [tab_ webController:mock_web_controller_ titleDidChange:kNewTabTitle];
  }

  void QueryAllHistory(history::QueryResults* results) {
    base::CancelableTaskTracker tracker;
    base::RunLoop run_loop;
    scoped_refptr<HistoryQueryResultsObserver> observer(
        new HistoryQueryResultsObserver(&run_loop));
    history_service_->QueryHistory(
        base::string16(), history::QueryOptions(),
        base::Bind(&HistoryQueryResultsObserver::ProcessResults, observer),
        &tracker);
    run_loop.Run();
    results->Swap(observer->results());
  }

  void CheckHistoryResult(const history::URLResult& historyResult,
                          const GURL& expectedUrl,
                          NSString* expectedTitle) {
    EXPECT_EQ(expectedUrl, historyResult.url());
    EXPECT_EQ(base::SysNSStringToUTF16(expectedTitle), historyResult.title());
  }

  void CheckCurrentItem(const GURL& expectedUrl, NSString* expectedTitle) {
    web::NavigationItem* item = [tab_ navigationManager]->GetVisibleItem();
    EXPECT_EQ(expectedUrl, item->GetURL());
    EXPECT_EQ(base::SysNSStringToUTF16(expectedTitle), item->GetTitle());
  }

  void CheckCurrentItem(const history::URLResult& historyResult) {
    web::NavigationItem* item = [tab_ navigationManager]->GetVisibleItem();
    CheckHistoryResult(historyResult, item->GetURL(),
                       base::SysUTF16ToNSString(item->GetTitle()));
  }

#ifndef NDEBUG
  // Method useful when debugging.
  void LogHistoryQueryResult(const history::QueryResults* results) {
    typedef history::QueryResults::URLResultVector::const_iterator iterator;
    for (iterator i = results->begin(); i != results->end(); ++i) {
      history::URLResult* result = *i;
      NSLog(@"title = %@; url = %@", base::SysUTF16ToNSString(result->title()),
            base::SysUTF8ToNSString(result->url().spec()));
    }
  }
#endif

  virtual bool UseOffTheRecordBrowserState() const { return false; }

 protected:
  web::TestWebThreadBundle thread_bundle_;
  IOSChromeScopedTestingLocalState local_state_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  base::scoped_nsobject<Tab> tab_;
  history::HistoryService* history_service_;  // weak
  CRWWebController* mock_web_controller_;     // weak
  base::scoped_nsobject<UIView> web_controller_view_;
  base::scoped_nsobject<ArrayTabModel> tabModel_;
  base::scoped_nsobject<id> mock_external_app_launcher_;
};

TEST_F(TabTest, AddToHistoryWithRedirect) {
  BrowseTo(GURL(kGoogleUserUrl), GURL(kGoogleRedirectUrl), kGoogleTitle);
  history::QueryResults results;
  QueryAllHistory(&results);
  EXPECT_EQ(1U, results.size());
  CheckHistoryResult(results[0], GURL(kGoogleRedirectUrl), kGoogleTitle);
  CheckCurrentItem(results[0]);
}

// TODO(crbug.com/378098): Disabled because forward/back is now implemented in
// CRWWebController, so this test cannot function with a mock CRWWebController.
// Rewrite and re-enable this test when it becomes a CRWWebController or
// NavigationManager test.
TEST_F(TabTest, DISABLED_BackAndForward) {
  BrowseTo(GURL(kGoogleUserUrl), GURL(kGoogleRedirectUrl), kGoogleTitle);
  BrowseTo(GURL(kOtherUserUrl), GURL(kOtherRedirectUrl), kOtherTitle);

  history::QueryResults results;
  QueryAllHistory(&results);
  EXPECT_EQ(2U, results.size());
  CheckHistoryResult(results[0], GURL(kOtherRedirectUrl), kOtherTitle);
  CheckHistoryResult(results[1], GURL(kGoogleRedirectUrl), kGoogleTitle);
  CheckCurrentItem(results[0]);
}

// TODO(crbug.com/378098): Disabled because navigation is no longer
// possible with a mock
// CRWWebController. Rewrite and re-enable this test when it becomes a
// CRWWebController test.
TEST_F(TabTest, DISABLED_NewTabInMiddleOfNavigation) {
  BrowseTo(GURL(kGoogleUserUrl), GURL(kGoogleRedirectUrl), kGoogleTitle);
  BrowseToNewTab();
  BrowseTo(GURL(kOtherUserUrl), GURL(kOtherRedirectUrl), kOtherTitle);

  // NOTE: NTP is not stored in the history.
  history::QueryResults results;
  QueryAllHistory(&results);
  EXPECT_EQ(2U, results.size());
  CheckHistoryResult(results[0], GURL(kOtherRedirectUrl), kOtherTitle);
  CheckHistoryResult(results[1], GURL(kGoogleRedirectUrl), kGoogleTitle);
}

TEST_F(TabTest, GetSuggestedFilenameFromContentDisposition) {
  // If possible, the filename should be generated from the content-disposition
  // header.
  GURL url(kValidFilenameUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders("HTTP 1.1 200 OK");
  headers->AddHeader(base::StringPrintf("Content-Type: application/pdf"));
  headers->AddHeader(base::StringPrintf("Content-Disposition: %s",
                                        kContentDispositionWithFilename));
  [[tab_ webController] webStateImpl]->OnHttpResponseHeadersReceived(
      headers.get(), url);
  BrowseTo(url, url, [NSString string]);
  EXPECT_NSEQ(@"suggested_filename.pdf",
              [[tab_ openInController] suggestedFilename]);
}

TEST_F(TabTest, GetSuggestedFilenameFromURL) {
  // If the content-disposition header does not specify a filename, this should
  // be extracted from the last component of the url.
  GURL url(kValidFilenameUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders("HTTP 1.1 200 OK");
  headers->AddHeader(base::StringPrintf("Content-Type: application/pdf"));
  headers->AddHeader(base::StringPrintf("Content-Disposition: %s",
                                        kContentDispositionWithoutFilename));
  [[tab_ webController] webStateImpl]->OnHttpResponseHeadersReceived(
      headers.get(), url);
  BrowseTo(url, url, [NSString string]);
  EXPECT_NSEQ(@"filename.pdf", [[tab_ openInController] suggestedFilename]);
}

TEST_F(TabTest, GetSuggestedFilenameFromDefaultName) {
  // If the filename cannot be extracted from the content disposition or from
  // the url, the default filename "Document.pdf" should be used.
  GURL url(kInvalidFilenameUrl);
  scoped_refptr<net::HttpResponseHeaders> headers =
      new net::HttpResponseHeaders("HTTP 1.1 200 OK");
  headers->AddHeader(base::StringPrintf("Content-Type: application/pdf"));
  [[tab_ webController] webStateImpl]->OnHttpResponseHeadersReceived(
      headers.get(), url);
  BrowseTo(url, url, [NSString string]);
  EXPECT_NSEQ(@"Document.pdf", [[tab_ openInController] suggestedFilename]);
}

// A separate test fixture is used to test opening external URLs using Google
// App Launcher. In any of the tests for this feature, scenarios have to be
// tested with the regular ChromeBrowserState AND the incognito
// ChromeBrowserState.
// In Incognito, -urlTriggersNativeAppLaunch:sourceURL should always return NO.
class TabOpenAppTest : public TabTest {
 protected:
  // Tests that calling |urlTriggersNativeAppLaunch:sourceURL:linkClicked| calls
  // |openURL:| the expected number of times. |return_value| is the value to be
  // returned from |openURL:|. |expected_result| is the value that is checked
  // for from |urlTriggersNativeAppLaunch:sourceURL:linkClicked|.
  void TestOpenNativeAppURL(const GURL& url,
                            BOOL return_value,
                            NSUInteger expected_tab_call_count,
                            BOOL expected_result) {
    ExpectWithMockedExternalAppLauncherOpenURL(
        return_value, expected_tab_call_count, ^{
          EXPECT_EQ(expected_result,
                    [tab_ urlTriggersNativeAppLaunch:url
                                           sourceURL:GURL("http://google.com")
                                         linkClicked:YES]);
        });
  }

  // Stubs out |openURL:| and checks how many times it was called.
  void ExpectWithMockedExternalAppLauncherOpenURL(
      BOOL return_value,
      NSUInteger expected_tab_call_count,
      ProceduralBlock expectation_block) {
    __block NSUInteger counter = 0;
    [mock_external_app_launcher_
                  onSelector:@selector(openURL:linkClicked:)
        callBlockExpectation:(id) ^ (const GURL& url, BOOL linkClicked) {
          ++counter;
          return return_value;
        }];
    expectation_block();
    EXPECT_EQ(expected_tab_call_count, counter);
    [mock_external_app_launcher_
        removeBlockExpectationOnSelector:@selector(openURL:linkClicked:)];
  }
};

// A version of TabOpenAppTests customized to use the off-the-record browser
// state (instead of the non-incognito one).
class TabOpenAppOffTheRecordTest : public TabOpenAppTest {
 private:
  bool UseOffTheRecordBrowserState() const override { return true; }
};

// Tests the opening of matching native apps.
TEST_F(TabOpenAppTest, testDummyURL) {
  EXPECT_FALSE([tab_ browserState]->IsOffTheRecord());

  GURL no_native_app_url("dummy string");
  TestOpenNativeAppURL(no_native_app_url, NO, 0, NO);
}

TEST_F(TabOpenAppTest, testURL) {
  EXPECT_FALSE([tab_ browserState]->IsOffTheRecord());

  GURL testURL("http://www.youtube.com/");
  // Fake metadata object to enable and disable autoopenlinks for testURL.
  base::scoped_nsobject<FakeNativeAppMetadata> metadata(
      [[FakeNativeAppMetadata alloc] init]);
  IOSChromeScopedTestingChromeBrowserProvider provider(
      base::MakeUnique<FakeChromeBrowserProvider>(metadata));
  // Turn auto open on.
  int expectedCallCount = 1;
  [metadata setShouldAutoOpenLinks:YES];
  TestOpenNativeAppURL(testURL, YES, expectedCallCount, YES);
  TestOpenNativeAppURL(testURL, NO, expectedCallCount, NO);

  // Turn auto open off.
  expectedCallCount = 0;
  [metadata setShouldAutoOpenLinks:NO];
  TestOpenNativeAppURL(testURL, NO, expectedCallCount, NO);
}

// TODO(crbug.com/330189): This test fails if Google Maps is installed (usually
// on device).
TEST_F(TabOpenAppTest, DISABLED_testResetShouldAutoOpenOnFailure) {
  EXPECT_FALSE([tab_ browserState]->IsOffTheRecord());

  // With a regular profile.
  GURL testURL("http://maps.google.com/");
  // Fake metadata object
  base::scoped_nsobject<FakeNativeAppMetadata> metadata(
      [[FakeNativeAppMetadata alloc] init]);

  // Turn auto open on.
  [metadata setShouldAutoOpenLinks:YES];
  int expectedCallCount = 2;
  TestOpenNativeAppURL(testURL, NO, expectedCallCount, NO);
  EXPECT_FALSE([metadata shouldAutoOpenLinks]);
}

// Tests the opening of matching native apps with off-the-record browser state.
TEST_F(TabOpenAppOffTheRecordTest, testDummyURL) {
  EXPECT_TRUE([tab_ browserState]->IsOffTheRecord());

  GURL no_native_app_url("dummy string");
  TestOpenNativeAppURL(no_native_app_url, NO, 0, NO);
}

TEST_F(TabOpenAppOffTheRecordTest, testURL) {
  EXPECT_TRUE([tab_ browserState]->IsOffTheRecord());

  // With a regular chrome browser state.
  GURL testURL("http://www.youtube.com/");
  // Mock metadata object to enable and disable autoopenlinks for testURL.
  base::scoped_nsobject<FakeNativeAppMetadata> metadata(
      [[FakeNativeAppMetadata alloc] init]);
  IOSChromeScopedTestingChromeBrowserProvider provider(
      base::MakeUnique<FakeChromeBrowserProvider>(metadata));

  // Turn auto open on.
  [metadata setShouldAutoOpenLinks:YES];
  TestOpenNativeAppURL(testURL, NO, 0, NO);

  // Turn auto open off.
  [metadata setShouldAutoOpenLinks:NO];
  TestOpenNativeAppURL(testURL, NO, 0, NO);
}

// TODO(crbug.com/330189): This test fails if Google Maps is installed (usually
// on device).
TEST_F(TabOpenAppOffTheRecordTest, DISABLED_testResetShouldAutoOpenOnFailure) {
  EXPECT_TRUE([tab_ browserState]->IsOffTheRecord());

  // With a regular profile.
  GURL testURL("http://maps.google.com/");
  // Fake metadata object.
  base::scoped_nsobject<FakeNativeAppMetadata> metadata(
      [[FakeNativeAppMetadata alloc] init]);

  // Turn auto open on.
  [metadata setShouldAutoOpenLinks:YES];
  int expectedCallCount = 2;
  TestOpenNativeAppURL(testURL, NO, expectedCallCount, NO);
  EXPECT_FALSE([metadata shouldAutoOpenLinks]);
}

class TestRequestGroupID : public BlockCleanupTest {
 public:
  void SetUp() override { BlockCleanupTest::SetUp(); }
  void TearDown() override { BlockCleanupTest::TearDown(); }
};

}  // namespace
