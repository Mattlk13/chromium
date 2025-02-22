// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Note that this file only tests the basic behavior of the cache counter, as in
// when it counts and when not, when result is nonzero and when not. It does not
// test whether the result of the counting is correct. This is the
// responsibility of a lower layer, and is tested in
// DiskCacheBackendTest.CalculateSizeOfAllEntries in net_unittests.

#include "ios/chrome/browser/browsing_data/cache_counter.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "ios/web/public/web_thread.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_transaction_factory.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class CacheCounterTest : public testing::Test {
 public:
  CacheCounterTest() {
    prefs_.registry()->RegisterIntegerPref(
        browsing_data::prefs::kDeleteTimePeriod,
        static_cast<int>(browsing_data::ALL_TIME));
    prefs_.registry()->RegisterBooleanPref(browsing_data::prefs::kDeleteCache,
                                           true);

    context_getter_ = browser_state_.GetRequestContext();
  }

  ~CacheCounterTest() override {}

  web::TestBrowserState* browser_state() { return &browser_state_; }

  PrefService* prefs() { return &prefs_; }

  void SetCacheDeletionPref(bool value) {
    prefs()->SetBoolean(browsing_data::prefs::kDeleteCache, value);
  }

  void SetDeletionPeriodPref(browsing_data::TimePeriod period) {
    prefs()->SetInteger(browsing_data::prefs::kDeleteTimePeriod,
                        static_cast<int>(period));
  }

  // Create a cache entry on the IO thread.
  void CreateCacheEntry() {
    current_operation_ = OPERATION_ADD_ENTRY;
    next_step_ = STEP_GET_BACKEND;

    web::WebThread::PostTask(web::WebThread::IO, FROM_HERE,
                             base::Bind(&CacheCounterTest::CacheOperationStep,
                                        base::Unretained(this), net::OK));
    WaitForIOThread();
  }

  // Clear the cache on the IO thread.
  void ClearCache() {
    current_operation_ = OPERATION_CLEAR_CACHE;
    next_step_ = STEP_GET_BACKEND;

    web::WebThread::PostTask(web::WebThread::IO, FROM_HERE,
                             base::Bind(&CacheCounterTest::CacheOperationStep,
                                        base::Unretained(this), net::OK));
    WaitForIOThread();
  }

  // Wait for IO thread operations, such as cache creation, counting, writing,
  // deletion etc.
  void WaitForIOThread() {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  // A callback method to be used by counters to report the result.
  void CountingCallback(
      std::unique_ptr<browsing_data::BrowsingDataCounter::Result> result) {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    finished_ = result->Finished();

    if (finished_) {
      result_ =
          static_cast<browsing_data::BrowsingDataCounter::FinishedResult*>(
              result.get())
              ->Value();
    }

    if (run_loop_ && finished_)
      run_loop_->Quit();
  }

  // Get the last reported counter result.
  browsing_data::BrowsingDataCounter::ResultInt GetResult() {
    DCHECK(finished_);
    return result_;
  }

 private:
  enum CacheOperation {
    OPERATION_ADD_ENTRY,
    OPERATION_CLEAR_CACHE,
  };

  enum CacheEntryCreationStep {
    STEP_GET_BACKEND,
    STEP_CLEAR_CACHE,
    STEP_CREATE_ENTRY,
    STEP_WRITE_DATA,
    STEP_CALLBACK,
    STEP_DONE
  };

  // One step in the process of creating a cache entry or clearing the cache.
  // Every step must be executed on IO thread after the previous one has
  // finished.
  void CacheOperationStep(int rv) {
    while (rv != net::ERR_IO_PENDING && next_step_ != STEP_DONE) {
      // The testing browser state uses a memory cache which should not cause
      // any errors.
      DCHECK_GE(rv, 0);

      switch (next_step_) {
        case STEP_GET_BACKEND: {
          next_step_ = current_operation_ == OPERATION_ADD_ENTRY
                           ? STEP_CREATE_ENTRY
                           : STEP_CLEAR_CACHE;

          net::HttpCache* http_cache = context_getter_->GetURLRequestContext()
                                           ->http_transaction_factory()
                                           ->GetCache();

          rv = http_cache->GetBackend(
              &backend_, base::Bind(&CacheCounterTest::CacheOperationStep,
                                    base::Unretained(this)));

          break;
        }

        case STEP_CLEAR_CACHE: {
          next_step_ = STEP_CALLBACK;

          DCHECK(backend_);
          rv = backend_->DoomAllEntries(base::Bind(
              &CacheCounterTest::CacheOperationStep, base::Unretained(this)));

          break;
        }

        case STEP_CREATE_ENTRY: {
          next_step_ = STEP_WRITE_DATA;

          DCHECK(backend_);
          rv = backend_->CreateEntry(
              "entry_key", &entry_,
              base::Bind(&CacheCounterTest::CacheOperationStep,
                         base::Unretained(this)));

          break;
        }

        case STEP_WRITE_DATA: {
          next_step_ = STEP_CALLBACK;

          std::string data = "entry data";
          scoped_refptr<net::StringIOBuffer> buffer =
              new net::StringIOBuffer(data);

          rv = entry_->WriteData(
              0, 0, buffer.get(), data.size(),
              base::Bind(&CacheCounterTest::CacheOperationStep,
                         base::Unretained(this)),
              true);

          break;
        }

        case STEP_CALLBACK: {
          next_step_ = STEP_DONE;

          if (current_operation_ == OPERATION_ADD_ENTRY)
            entry_->Close();

          web::WebThread::PostTask(
              web::WebThread::UI, FROM_HERE,
              base::Bind(&CacheCounterTest::Callback, base::Unretained(this)));

          break;
        }

        case STEP_DONE: {
          NOTREACHED();
        }
      }
    }
  }

  // General completion callback.
  void Callback() {
    DCHECK_CURRENTLY_ON(web::WebThread::UI);
    if (run_loop_)
      run_loop_->Quit();
  }

  web::TestWebThreadBundle bundle_;
  std::unique_ptr<base::RunLoop> run_loop_;

  CacheOperation current_operation_;
  CacheEntryCreationStep next_step_;

  scoped_refptr<net::URLRequestContextGetter> context_getter_;
  disk_cache::Backend* backend_;
  disk_cache::Entry* entry_;

  bool finished_;
  browsing_data::BrowsingDataCounter::ResultInt result_;

  web::TestBrowserState browser_state_;
  TestingPrefServiceSimple prefs_;
};

// Tests that for the empty cache, the result is zero.
TEST_F(CacheCounterTest, Empty) {
  CacheCounter counter(browser_state());
  counter.Init(prefs(), base::Bind(&CacheCounterTest::CountingCallback,
                                   base::Unretained(this)));
  counter.Restart();

  WaitForIOThread();
  EXPECT_EQ(0u, GetResult());
}

// Tests that for a non-empty cache, the result is nonzero, and after deleting
// its contents, it's zero again. Note that the exact value of the result
// is tested in DiskCacheBackendTest.CalculateSizeOfAllEntries.
TEST_F(CacheCounterTest, BeforeAndAfterClearing) {
  CreateCacheEntry();

  CacheCounter counter(browser_state());
  counter.Init(prefs(), base::Bind(&CacheCounterTest::CountingCallback,
                                   base::Unretained(this)));
  counter.Restart();

  WaitForIOThread();
  EXPECT_NE(0u, GetResult());

  ClearCache();
  counter.Restart();

  WaitForIOThread();
  EXPECT_EQ(0u, GetResult());
}

// Tests that the counter starts counting automatically when the deletion
// pref changes to true.
TEST_F(CacheCounterTest, PrefChanged) {
  SetCacheDeletionPref(false);

  CacheCounter counter(browser_state());
  counter.Init(prefs(), base::Bind(&CacheCounterTest::CountingCallback,
                                   base::Unretained(this)));
  SetCacheDeletionPref(true);

  WaitForIOThread();
  EXPECT_EQ(0u, GetResult());
}

// Tests that the counter does not count if the deletion preference is false.
TEST_F(CacheCounterTest, PrefIsFalse) {
  SetCacheDeletionPref(false);

  CacheCounter counter(browser_state());
  counter.Init(prefs(), base::Bind(&CacheCounterTest::CountingCallback,
                                   base::Unretained(this)));
  counter.Restart();

  EXPECT_FALSE(counter.pending());
}

// Tests that the counting is restarted when the time period changes. Currently,
// the results should be the same for every period. This is because the counter
// always counts the size of the entire cache, and it is up to the UI
// to interpret it as exact value or upper bound.
TEST_F(CacheCounterTest, PeriodChanged) {
  CreateCacheEntry();

  CacheCounter counter(browser_state());
  counter.Init(prefs(), base::Bind(&CacheCounterTest::CountingCallback,
                                   base::Unretained(this)));

  SetDeletionPeriodPref(browsing_data::LAST_HOUR);
  WaitForIOThread();
  browsing_data::BrowsingDataCounter::ResultInt result = GetResult();

  SetDeletionPeriodPref(browsing_data::LAST_DAY);
  WaitForIOThread();
  EXPECT_EQ(result, GetResult());

  SetDeletionPeriodPref(browsing_data::LAST_WEEK);
  WaitForIOThread();
  EXPECT_EQ(result, GetResult());

  SetDeletionPeriodPref(browsing_data::FOUR_WEEKS);
  WaitForIOThread();
  EXPECT_EQ(result, GetResult());

  SetDeletionPeriodPref(browsing_data::ALL_TIME);
  WaitForIOThread();
  EXPECT_EQ(result, GetResult());
}

}  // namespace
