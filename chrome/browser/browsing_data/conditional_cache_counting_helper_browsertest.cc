// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <set>
#include <string>

#include "base/run_loop.h"
#include "chrome/browser/browsing_data/cache_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/browsing_data/content/conditional_cache_counting_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"

using browsing_data::ConditionalCacheCountingHelper;
using content::BrowserThread;

class ConditionalCacheCountingHelperBrowserTest : public InProcessBrowserTest {
 public:
  const int64_t kTimeoutMs = 10;

  void SetUpOnMainThread() override {
    count_callback_ =
        base::Bind(&ConditionalCacheCountingHelperBrowserTest::CountCallback,
                   base::Unretained(this));

    cache_util_ = base::MakeUnique<CacheTestUtil>(
        content::BrowserContext::GetDefaultStoragePartition(
            browser()->profile()));
  }

  void TearDownOnMainThread() override { cache_util_.reset(); }

  void CountCallback(int64_t size) {
    // Negative values represent an unexpected error.
    DCHECK(size >= 0 || size == net::ERR_ABORTED);
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    last_size_ = size;

    if (run_loop_)
      run_loop_->Quit();
  }

  void WaitForTasksOnIOThread() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  void CountEntries(base::Time begin_time, base::Time end_time) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    last_size_ = -1;
    auto helper = ConditionalCacheCountingHelper::CreateForRange(
        cache_util_->partition(), begin_time, end_time);
    helper->CountAndDestroySelfWhenFinished(count_callback_);
  }

  int64_t GetResult() {
    DCHECK_GT(last_size_, 0);
    return last_size_;
  }

  int64_t GetResultOrError() { return last_size_; }

  CacheTestUtil* GetCacheTestUtil() { return cache_util_.get(); }

 private:
  ConditionalCacheCountingHelper::CacheCountCallback count_callback_;
  std::unique_ptr<base::RunLoop> run_loop_;
  std::unique_ptr<CacheTestUtil> cache_util_;

  int64_t last_size_;
};

// Tests that ConditionalCacheCountingHelper only counts those cache entries
// that match the condition.
IN_PROC_BROWSER_TEST_F(ConditionalCacheCountingHelperBrowserTest, Count) {
  // Create 5 entries.
  std::set<std::string> keys1 = {"1", "2", "3", "4", "5"};

  base::Time t1 = base::Time::Now();
  GetCacheTestUtil()->CreateCacheEntries(keys1);

  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(kTimeoutMs));
  // base::Time t2 = base::Time::Now();

  std::set<std::string> keys2 = {"6", "7"};
  GetCacheTestUtil()->CreateCacheEntries(keys2);

  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(kTimeoutMs));
  base::Time t3 = base::Time::Now();

  // TODO(dullweber): Add test for time ranges when GetEntrySize() is done.

  // Count all entries.
  CountEntries(t1, t3);
  WaitForTasksOnIOThread();
  int64_t size_1_3 = GetResult();

  // Count everything
  CountEntries(base::Time(), base::Time::Max());
  WaitForTasksOnIOThread();
  EXPECT_EQ(size_1_3, GetResult());
}
