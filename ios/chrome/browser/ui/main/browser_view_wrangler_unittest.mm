// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main/browser_view_wrangler.h"

#import <UIKit/UIKit.h>

#include "base/mac/scoped_nsobject.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/platform_test.h"

namespace {

class BrowserViewWranglerTest : public PlatformTest {
 protected:
  BrowserViewWranglerTest() {
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();
    chrome_browser_state_->CreateBookmarkModel(false);
    bookmarks::BookmarkModel* bookmark_model =
        ios::BookmarkModelFactory::GetForBrowserState(
            chrome_browser_state_.get());
    bookmarks::test::WaitForBookmarkModelToLoad(bookmark_model);
  }

  // SessionWindow, used to create the TabModel, needs to run on the web thread.
  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
};

TEST_F(BrowserViewWranglerTest, TestInitNilObserver) {
  base::scoped_nsobject<BrowserViewWrangler> wrangler(
      [[BrowserViewWrangler alloc]
          initWithBrowserState:chrome_browser_state_.get()
              tabModelObserver:nil]);

  // Test that BVC and tab model are created on demand.
  BrowserViewController* bvc = [wrangler mainBVC];
  EXPECT_NE(bvc, nil);

  TabModel* tabModel = [wrangler mainTabModel];
  EXPECT_NE(tabModel, nil);

  // Test that once created the BVC and tab model aren't re-created.
  EXPECT_EQ(bvc, [wrangler mainBVC]);
  EXPECT_EQ(tabModel, [wrangler mainTabModel]);

  // Test that the OTR objects are (a) OTR and (b) not the same as the non-OTR
  // objects.
  EXPECT_NE(bvc, [wrangler otrBVC]);
  EXPECT_NE(tabModel, [wrangler otrTabModel]);
  EXPECT_TRUE([wrangler otrTabModel].browserState->IsOffTheRecord());
}

}  // namespace
