// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_web_state_observer.h"

#include "base/memory/ptr_util.h"
#include "components/reading_list/ios/reading_list_model_impl.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/public/web_state/web_state.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kTestURL[] = "http://foo.bar";
const char kTestTitle[] = "title";
const char kTestDistilledPath[] = "distilled/page.html";
}

// A Test navigation manager that checks if Reload was called.
class TestNavigationManager : public web::TestNavigationManager {
 public:
  void Reload(bool check_for_repost) override { reload_called_ = true; }
  bool ReloadCalled() { return reload_called_; }

 private:
  bool reload_called_ = false;
};

// A Test navigation manager that remembers the last opened parameters.
class TestWebState : public web::TestWebState {
 public:
  void OpenURL(const web::WebState::OpenURLParams& params) override {
    last_opened_url_ = params.url;
  }
  const GURL& LastOpenedUrl() { return last_opened_url_; }

 private:
  GURL last_opened_url_;
};

// Test fixture to test loading of Reading list entries.
class ReadingListWebStateObserverTest : public web::WebTest {
 public:
  ReadingListWebStateObserverTest() {
    auto test_navigation_manager = base::MakeUnique<TestNavigationManager>();
    test_navigation_manager_ = test_navigation_manager.get();
    pending_item_ = web::NavigationItem::Create();
    last_committed_item_ = web::NavigationItem::Create();
    test_navigation_manager->SetPendingItem(pending_item_.get());
    test_navigation_manager->SetLastCommittedItem(last_committed_item_.get());
    test_web_state_.SetNavigationManager(std::move(test_navigation_manager));
    reading_list_model_ =
        base::MakeUnique<ReadingListModelImpl>(nullptr, nullptr);
    reading_list_model_->AddEntry(GURL(kTestURL), kTestTitle);
    ReadingListWebStateObserver::FromWebState(&test_web_state_,
                                              reading_list_model_.get());
  }

 protected:
  std::unique_ptr<web::NavigationItem> pending_item_;
  std::unique_ptr<web::NavigationItem> last_committed_item_;
  std::unique_ptr<ReadingListModelImpl> reading_list_model_;
  TestNavigationManager* test_navigation_manager_;
  TestWebState test_web_state_;
};

// Tests that failing loading an online version does not mark it read.
TEST_F(ReadingListWebStateObserverTest, TestLoadReadingListFailure) {
  GURL url(kTestURL);
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  test_navigation_manager_->GetPendingItem()->SetURL(url);
  test_web_state_.SetLoading(true);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  test_web_state_.SetLoading(false);

  EXPECT_FALSE(test_navigation_manager_->ReloadCalled());
  // Check that |GetLastCommittedItem()| has not been altered.
  EXPECT_EQ(test_navigation_manager_->GetLastCommittedItem()->GetVirtualURL(),
            GURL());
  EXPECT_EQ(test_navigation_manager_->GetLastCommittedItem()->GetURL(), GURL());
  EXPECT_FALSE(entry->IsRead());
}

// Tests that loading an online version of an entry.
TEST_F(ReadingListWebStateObserverTest, TestLoadReadingListOnline) {
  GURL url(kTestURL);
  std::string distilled_path = kTestDistilledPath;
  reading_list_model_->SetEntryDistilledPath(url,
                                             base::FilePath(distilled_path));
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  GURL distilled_url =
      reading_list::DistilledURLForPath(entry->DistilledPath(), entry->URL());

  test_navigation_manager_->GetPendingItem()->SetURL(url);
  test_web_state_.SetLoading(true);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  test_web_state_.SetLoading(false);

  EXPECT_FALSE(test_navigation_manager_->ReloadCalled());
  // Check that |GetLastCommittedItem()| has not been altered.
  EXPECT_EQ(test_navigation_manager_->GetLastCommittedItem()->GetVirtualURL(),
            GURL());
  EXPECT_EQ(test_navigation_manager_->GetLastCommittedItem()->GetURL(), GURL());
  EXPECT_TRUE(entry->IsRead());
}

// Tests that loading a distilled version of an entry from a commited entry.
TEST_F(ReadingListWebStateObserverTest, TestLoadReadingListDistilledCommitted) {
  GURL url(kTestURL);
  std::string distilled_path = kTestDistilledPath;
  reading_list_model_->SetEntryDistilledPath(url,
                                             base::FilePath(distilled_path));
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  GURL distilled_url =
      reading_list::DistilledURLForPath(entry->DistilledPath(), entry->URL());

  test_navigation_manager_->GetPendingItem()->SetURL(url);
  test_web_state_.SetLoading(true);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  test_web_state_.SetLoading(false);

  EXPECT_FALSE(test_navigation_manager_->ReloadCalled());
  EXPECT_EQ(test_web_state_.LastOpenedUrl(), distilled_url);
  EXPECT_TRUE(entry->IsRead());
}

// Tests that loading a distilled version of an entry.
TEST_F(ReadingListWebStateObserverTest, TestLoadReadingListDistilledPending) {
  GURL url(kTestURL);
  std::string distilled_path = kTestDistilledPath;
  reading_list_model_->SetEntryDistilledPath(url,
                                             base::FilePath(distilled_path));
  const ReadingListEntry* entry = reading_list_model_->GetEntryByURL(url);
  GURL distilled_url =
      reading_list::DistilledURLForPath(entry->DistilledPath(), entry->URL());

  test_navigation_manager_->SetPendingItem(nil);
  test_navigation_manager_->GetLastCommittedItem()->SetURL(url);
  test_web_state_.SetLoading(true);
  test_web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  test_web_state_.SetLoading(false);

  EXPECT_TRUE(test_navigation_manager_->ReloadCalled());
  EXPECT_EQ(test_navigation_manager_->GetLastCommittedItem()->GetVirtualURL(),
            url);
  EXPECT_EQ(test_navigation_manager_->GetLastCommittedItem()->GetURL(),
            distilled_url);

  EXPECT_TRUE(entry->IsRead());
}
