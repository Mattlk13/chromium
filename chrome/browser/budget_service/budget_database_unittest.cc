// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/budget_service/budget_database.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/budget_service/budget.pb.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/test/base/testing_profile.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/leveldb_proto/proto_database_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/array.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

const double kDefaultExpirationInHours = 96;
const double kDefaultEngagement = 30.0;

const char kTestOrigin[] = "https://example.com";

}  // namespace

class BudgetDatabaseTest : public ::testing::Test {
 public:
  BudgetDatabaseTest()
      : success_(false),
        db_(&profile_,
            profile_.GetPath().Append(FILE_PATH_LITERAL("BudgetDatabase")),
            base::ThreadTaskRunnerHandle::Get()),
        origin_(url::Origin(GURL(kTestOrigin))) {}

  void WriteBudgetComplete(base::Closure run_loop_closure,
                           blink::mojom::BudgetServiceErrorType error,
                           bool success) {
    success_ = (error == blink::mojom::BudgetServiceErrorType::NONE) && success;
    run_loop_closure.Run();
  }

  // Spend budget for the origin.
  bool SpendBudget(double amount) {
    base::RunLoop run_loop;
    db_.SpendBudget(origin(), amount,
                    base::Bind(&BudgetDatabaseTest::WriteBudgetComplete,
                               base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return success_;
  }

  void GetBudgetDetailsComplete(
      base::Closure run_loop_closure,
      blink::mojom::BudgetServiceErrorType error,
      std::vector<blink::mojom::BudgetStatePtr> predictions) {
    success_ = (error == blink::mojom::BudgetServiceErrorType::NONE);
    prediction_.swap(predictions);
    run_loop_closure.Run();
  }

  // Get the full set of budget predictions for the origin.
  void GetBudgetDetails() {
    base::RunLoop run_loop;
    db_.GetBudgetDetails(
        origin(), base::Bind(&BudgetDatabaseTest::GetBudgetDetailsComplete,
                             base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  Profile* profile() { return &profile_; }
  const url::Origin& origin() const { return origin_; }

  // Setup a test clock so that the tests can control time.
  base::SimpleTestClock* SetClockForTesting() {
    base::SimpleTestClock* clock = new base::SimpleTestClock();
    db_.SetClockForTesting(base::WrapUnique(clock));
    return clock;
  }

  void SetSiteEngagementScore(double score) {
    SiteEngagementService* service = SiteEngagementService::Get(&profile_);
    service->ResetScoreForURL(GURL(kTestOrigin), score);
  }

 protected:
  base::HistogramTester* GetHistogramTester() { return &histogram_tester_; }
  bool success_;
  std::vector<blink::mojom::BudgetStatePtr> prediction_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<budget_service::Budget> budget_;
  TestingProfile profile_;
  BudgetDatabase db_;
  base::HistogramTester histogram_tester_;
  const url::Origin origin_;
};

TEST_F(BudgetDatabaseTest, GetBudgetNoBudgetOrSES) {
  GetBudgetDetails();
  ASSERT_TRUE(success_);
  ASSERT_EQ(2U, prediction_.size());
  EXPECT_EQ(0, prediction_[0]->budget_at);
}

TEST_F(BudgetDatabaseTest, AddEngagementBudgetTest) {
  base::SimpleTestClock* clock = SetClockForTesting();
  base::Time expiration_time =
      clock->Now() + base::TimeDelta::FromHours(kDefaultExpirationInHours);

  // Set the default site engagement.
  SetSiteEngagementScore(kDefaultEngagement);

  // The budget should include a full share of the engagement.
  GetBudgetDetails();
  ASSERT_TRUE(success_);
  ASSERT_EQ(2U, prediction_.size());
  ASSERT_EQ(kDefaultEngagement, prediction_[0]->budget_at);
  ASSERT_EQ(0, prediction_[1]->budget_at);
  ASSERT_EQ(expiration_time.ToDoubleT(), prediction_[1]->time);

  // Advance time 1 day and add more engagement budget.
  clock->Advance(base::TimeDelta::FromDays(1));
  GetBudgetDetails();

  // The budget should now have 1 full share plus 1 daily budget.
  ASSERT_TRUE(success_);
  ASSERT_EQ(3U, prediction_.size());
  double daily_budget = kDefaultEngagement * 24 / kDefaultExpirationInHours;
  ASSERT_DOUBLE_EQ(kDefaultEngagement + daily_budget,
                   prediction_[0]->budget_at);
  ASSERT_DOUBLE_EQ(daily_budget, prediction_[1]->budget_at);
  ASSERT_EQ(expiration_time.ToDoubleT(), prediction_[1]->time);
  ASSERT_EQ(0, prediction_[2]->budget_at);
  ASSERT_EQ((expiration_time + base::TimeDelta::FromDays(1)).ToDoubleT(),
            prediction_[2]->time);

  // Advance time by 59 minutes and check that no engagement budget is added
  // since budget should only be added for > 1 hour increments.
  clock->Advance(base::TimeDelta::FromMinutes(59));
  GetBudgetDetails();

  // The budget should be the same as before the attempted add.
  ASSERT_TRUE(success_);
  ASSERT_EQ(3U, prediction_.size());
  ASSERT_DOUBLE_EQ(kDefaultEngagement + daily_budget,
                   prediction_[0]->budget_at);
}

TEST_F(BudgetDatabaseTest, SpendBudgetTest) {
  base::SimpleTestClock* clock = SetClockForTesting();

  // Set the default site engagement.
  SetSiteEngagementScore(kDefaultEngagement);

  // Intialize the budget with several chunks.
  GetBudgetDetails();
  clock->Advance(base::TimeDelta::FromDays(1));
  GetBudgetDetails();
  clock->Advance(base::TimeDelta::FromDays(1));
  GetBudgetDetails();

  // Spend an amount of budget less than kDefaultEngagement.
  ASSERT_TRUE(SpendBudget(1));
  GetBudgetDetails();

  // There should still be three chunks of budget of size kDefaultEngagement-1,
  // kDefaultEngagement, and kDefaultEngagement.
  ASSERT_EQ(4U, prediction_.size());
  double daily_budget = kDefaultEngagement * 24 / kDefaultExpirationInHours;
  ASSERT_DOUBLE_EQ(kDefaultEngagement + 2 * daily_budget - 1,
                   prediction_[0]->budget_at);
  ASSERT_DOUBLE_EQ(daily_budget * 2, prediction_[1]->budget_at);
  ASSERT_DOUBLE_EQ(daily_budget, prediction_[2]->budget_at);
  ASSERT_DOUBLE_EQ(0, prediction_[3]->budget_at);

  // Now spend enough that it will use up the rest of the first chunk and all of
  // the second chunk, but not all of the third chunk.
  ASSERT_TRUE(SpendBudget(kDefaultEngagement + daily_budget));
  GetBudgetDetails();
  ASSERT_EQ(2U, prediction_.size());
  ASSERT_DOUBLE_EQ(daily_budget - 1, prediction_[0]->budget_at);

  // Validate that the code returns false if SpendBudget tries to spend more
  // budget than the origin has.
  EXPECT_FALSE(SpendBudget(kDefaultEngagement));
  GetBudgetDetails();
  ASSERT_EQ(2U, prediction_.size());
  ASSERT_DOUBLE_EQ(daily_budget - 1, prediction_[0]->budget_at);

  // Advance time until the last remaining chunk should be expired, then query
  // for the full engagement worth of budget.
  clock->Advance(base::TimeDelta::FromHours(kDefaultExpirationInHours + 1));
  EXPECT_TRUE(SpendBudget(kDefaultEngagement));
}

// There are times when a device's clock could move backwards in time, either
// due to hardware issues or user actions. Test here to make sure that even if
// time goes backwards and then forwards again, the origin isn't granted extra
// budget.
TEST_F(BudgetDatabaseTest, GetBudgetNegativeTime) {
  base::SimpleTestClock* clock = SetClockForTesting();

  // Set the default site engagement.
  SetSiteEngagementScore(kDefaultEngagement);

  // Initialize the budget with two chunks.
  GetBudgetDetails();
  clock->Advance(base::TimeDelta::FromDays(1));
  GetBudgetDetails();

  // Save off the budget total.
  ASSERT_EQ(3U, prediction_.size());
  double budget = prediction_[0]->budget_at;

  // Move the clock backwards in time to before the budget awards.
  clock->SetNow(clock->Now() - base::TimeDelta::FromDays(5));

  // Make sure the budget is the same.
  GetBudgetDetails();
  ASSERT_EQ(3U, prediction_.size());
  ASSERT_EQ(budget, prediction_[0]->budget_at);

  // Now move the clock back to the original time and check that no extra budget
  // is awarded.
  clock->SetNow(clock->Now() + base::TimeDelta::FromDays(5));
  GetBudgetDetails();
  ASSERT_EQ(3U, prediction_.size());
  ASSERT_EQ(budget, prediction_[0]->budget_at);
}

TEST_F(BudgetDatabaseTest, CheckBackgroundBudgetHistogram) {
  base::SimpleTestClock* clock = SetClockForTesting();

  // Set the default site engagement.
  SetSiteEngagementScore(kDefaultEngagement);

  // Initialize the budget with some interesting chunks: 30 budget (full
  // engagement), 15 budget (half of the engagement), 0 budget (less than an
  // hour), and then after the first two expire, another 30 budget.
  GetBudgetDetails();
  clock->Advance(base::TimeDelta::FromHours(kDefaultExpirationInHours / 2));
  GetBudgetDetails();
  clock->Advance(base::TimeDelta::FromMinutes(59));
  GetBudgetDetails();
  clock->Advance(base::TimeDelta::FromHours(kDefaultExpirationInHours + 1));
  GetBudgetDetails();

  // The BackgroundBudget UMA is recorded when budget is added to the origin.
  // This can happen a maximum of once per hour so there should be two entries.
  std::vector<base::Bucket> buckets =
      GetHistogramTester()->GetAllSamples("PushMessaging.BackgroundBudget");
  ASSERT_EQ(2U, buckets.size());
  // First bucket is for full engagement, which should have 2 entries.
  EXPECT_EQ(kDefaultEngagement, buckets[0].min);
  EXPECT_EQ(2, buckets[0].count);
  // Second bucket is for 1.5 * engagement, which should have 1 entry.
  EXPECT_EQ(kDefaultEngagement * 1.5, buckets[1].min);
  EXPECT_EQ(1, buckets[1].count);
}

TEST_F(BudgetDatabaseTest, CheckEngagementHistograms) {
  base::SimpleTestClock* clock = SetClockForTesting();

  // Set the engagement to twice the cost of an action.
  double cost = 2;
  double engagement = cost * 2;
  SetSiteEngagementScore(engagement);

  // Get the budget, which will award a chunk of budget equal to engagement.
  GetBudgetDetails();

  // Now spend the budget to trigger the UMA recording the SES score. The first
  // call shouldn't write any UMA. The second should write a lowSES entry, and
  // the third should write a noSES entry.
  ASSERT_TRUE(SpendBudget(cost));
  ASSERT_TRUE(SpendBudget(cost));
  ASSERT_FALSE(SpendBudget(cost));

  // Advance the clock by 12 days (to guarantee a full new engagement grant)
  // then change the SES score to get a different UMA entry, then spend the
  // budget again.
  clock->Advance(base::TimeDelta::FromDays(12));
  GetBudgetDetails();
  SetSiteEngagementScore(engagement * 2);
  ASSERT_TRUE(SpendBudget(cost));
  ASSERT_TRUE(SpendBudget(cost));
  ASSERT_FALSE(SpendBudget(cost));

  // Now check the UMA. Both UMA should have 2 buckets with 1 entry each.
  std::vector<base::Bucket> no_budget_buckets =
      GetHistogramTester()->GetAllSamples("PushMessaging.SESForNoBudgetOrigin");
  ASSERT_EQ(2U, no_budget_buckets.size());
  EXPECT_EQ(engagement, no_budget_buckets[0].min);
  EXPECT_EQ(1, no_budget_buckets[0].count);
  EXPECT_EQ(engagement * 2, no_budget_buckets[1].min);
  EXPECT_EQ(1, no_budget_buckets[1].count);

  std::vector<base::Bucket> low_budget_buckets =
      GetHistogramTester()->GetAllSamples(
          "PushMessaging.SESForLowBudgetOrigin");
  ASSERT_EQ(2U, low_budget_buckets.size());
  EXPECT_EQ(engagement, low_budget_buckets[0].min);
  EXPECT_EQ(1, low_budget_buckets[0].count);
  EXPECT_EQ(engagement * 2, low_budget_buckets[1].min);
  EXPECT_EQ(1, low_budget_buckets[1].count);
}
