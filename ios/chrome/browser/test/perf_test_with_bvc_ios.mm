// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/test/perf_test_with_bvc_ios.h"

#import <UIKit/UIKit.h>

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "ios/chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/sessions/session_service.h"
#import "ios/chrome/browser/sessions/session_window.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/browser_view_controller_dependency_factory.h"
#import "ios/chrome/browser/ui/preload_controller.h"
#import "ios/chrome/browser/web/chrome_web_client.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"

// Subclass of PrerenderController so it isn't actually used. Using a mock for
// this makes cleanup on shutdown simpler, by minimizing the number of profile
// observers registered with the profiles.  The profile observers have to be
// deallocated before the profiles themselves, but in practice it is very hard
// to ensure that happens.  Also, for performance testing, not having the
// PrerenderController makes the test far simpler to analyze.
namespace {
static GURL emptyGurl_ = GURL("foo", 3, url::Parsed(), false);
}

@interface TestPreloadController : PreloadController
- (Tab*)releasePrerenderContents;
@end

@implementation TestPreloadController

- (Tab*)releasePrerenderContents {
  return nil;
}

- (id<PreloadControllerDelegate>)delegate {
  return nil;
}

- (GURL)prerenderedURL {
  return emptyGurl_;
}
@end

// Subclass the factory that creates the PreloadController for BVC to return
// the TestPrerenderController.
@interface TestBVCDependencyFactory : BrowserViewControllerDependencyFactory
- (PreloadController*)newPreloadController;
@end

@implementation TestBVCDependencyFactory
- (PreloadController*)newPreloadController {
  return [[TestPreloadController alloc] init];
}
@end

PerfTestWithBVC::PerfTestWithBVC(std::string testGroup)
    : PerfTest(testGroup),
      slow_teardown_(false),
      web_client_(base::MakeUnique<ChromeWebClient>()),
      provider_(ios::CreateChromeBrowserProvider()) {}

PerfTestWithBVC::PerfTestWithBVC(std::string testGroup,
                                 std::string firstLabel,
                                 std::string averageLabel,
                                 bool isWaterfall,
                                 bool verbose,
                                 bool slowTeardown,
                                 int repeat)
    : PerfTest(testGroup,
               firstLabel,
               averageLabel,
               isWaterfall,
               verbose,
               repeat),
      slow_teardown_(slowTeardown),
      web_client_(base::MakeUnique<ChromeWebClient>()),
      provider_(ios::CreateChromeBrowserProvider()) {}

PerfTestWithBVC::~PerfTestWithBVC() {}

void PerfTestWithBVC::SetUp() {
  PerfTest::SetUp();

  // Set up the ChromeBrowserState instances.
  TestChromeBrowserState::Builder test_cbs_builder;
  test_cbs_builder.AddTestingFactory(
      ios::TemplateURLServiceFactory::GetInstance(),
      ios::TemplateURLServiceFactory::GetDefaultFactory());
  test_cbs_builder.AddTestingFactory(
      ios::AutocompleteClassifierFactory::GetInstance(),
      ios::AutocompleteClassifierFactory::GetDefaultFactory());
  chrome_browser_state_ = test_cbs_builder.Build();
  chrome_browser_state_->CreateBookmarkModel(false);
  bookmarks::test::WaitForBookmarkModelToLoad(
      ios::BookmarkModelFactory::GetForBrowserState(
          chrome_browser_state_.get()));
  ASSERT_TRUE(chrome_browser_state_->CreateHistoryService(false));

  // Force creation of AutocompleteClassifier instance.
  ios::AutocompleteClassifierFactory::GetForBrowserState(
      chrome_browser_state_.get());

  // Use the session to create a window which will contain the tab models.
  SessionWindowIOS* sessionWindow = [[SessionServiceIOS sharedService]
      loadWindowForBrowserState:chrome_browser_state_.get()];

  // Tab models. The off-the-record (OTR) tab model is required for the stack
  // view controller, which is created in OpenStackView().
  tab_model_.reset([[TabModel alloc]
      initWithSessionWindow:sessionWindow
             sessionService:[SessionServiceIOS sharedService]
               browserState:chrome_browser_state_.get()]);
  otr_tab_model_.reset([[TabModel alloc]
      initWithSessionWindow:sessionWindow
             sessionService:[SessionServiceIOS sharedService]
               browserState:chrome_browser_state_
                                ->GetOffTheRecordChromeBrowserState()]);

  // Create the browser view controller with its testing factory.
  bvc_factory_.reset([[TestBVCDependencyFactory alloc]
      initWithBrowserState:chrome_browser_state_.get()]);
  bvc_.reset([[BrowserViewController alloc]
       initWithTabModel:tab_model_
           browserState:chrome_browser_state_.get()
      dependencyFactory:bvc_factory_]);
  [bvc_ setActive:YES];

  // Create a real window to give to the browser view controller.
  window_.reset(
      [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]]);
  [window_ makeKeyAndVisible];
  [window_ addSubview:[bvc_ view]];
  [[bvc_ view] setFrame:[[UIScreen mainScreen] bounds]];
}

void PerfTestWithBVC::TearDown() {
  [[bvc_ tabModel] closeAllTabs];
  [[bvc_ view] removeFromSuperview];

  // Documented example of how to clear out the browser view controller
  // and its associated data.
  window_.reset();
  [bvc_ browserStateDestroyed];
  bvc_.reset();
  bvc_factory_.reset();
  tab_model_.reset();
  [otr_tab_model_ browserStateDestroyed];
  otr_tab_model_.reset();

  // The base class |TearDown| method calls the run loop so the
  // NSAutoreleasePool can drain. This needs to be done before
  // |chrome_browser_state_| can be cleared. For tests that allocate more
  // objects, more runloop time may be required.
  if (slow_teardown_)
    SpinRunLoop(.5);
  PerfTest::TearDown();

  // The profiles can be deallocated only after the BVC has been deallocated.
  chrome_browser_state_.reset();
}
