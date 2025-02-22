// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/renderer/subresource_filter_agent.h"

#include <memory>
#include <utility>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/common/subresource_filter_messages.h"
#include "components/subresource_filter/content/renderer/ruleset_dealer.h"
#include "components/subresource_filter/core/common/scoped_timers.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebDocumentSubresourceFilter.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "url/gurl.h"

namespace subresource_filter {

namespace {

// The SubresourceFilterAgent with its dependencies on Blink mocked out.
//
// This approach is somewhat rudimentary, but appears to be the best compromise
// considering the alternatives:
//  -- Passing in a TestRenderFrame would itself require bringing up a
//     significant number of supporting classes.
//  -- Using a RenderViewTest would not allow having any non-filtered resource
//     loads due to not having a child thread and ResourceDispatcher.
// The real implementations of the mocked-out methods are exercised in:
//   chrome/browser/subresource_filter/subresource_filter_browsertest.cc.
class SubresourceFilterAgentUnderTest : public SubresourceFilterAgent {
 public:
  explicit SubresourceFilterAgentUnderTest(RulesetDealer* ruleset_dealer)
      : SubresourceFilterAgent(nullptr /* RenderFrame */, ruleset_dealer) {}
  ~SubresourceFilterAgentUnderTest() = default;

  MOCK_METHOD0(GetAncestorDocumentURLs, std::vector<GURL>());
  MOCK_METHOD0(OnSetSubresourceFilterForCommittedLoadCalled, void());
  MOCK_METHOD0(SignalFirstSubresourceDisallowedForCommittedLoad, void());
  MOCK_METHOD2(SendDocumentLoadStatistics,
               void(base::TimeDelta, base::TimeDelta));

  void SetSubresourceFilterForCommittedLoad(
      std::unique_ptr<blink::WebDocumentSubresourceFilter> filter) override {
    last_injected_filter_ = std::move(filter);
    OnSetSubresourceFilterForCommittedLoadCalled();
  }

  blink::WebDocumentSubresourceFilter* filter() {
    return last_injected_filter_.get();
  }

  std::unique_ptr<blink::WebDocumentSubresourceFilter> TakeFilter() {
    return std::move(last_injected_filter_);
  }

 private:
  std::unique_ptr<blink::WebDocumentSubresourceFilter> last_injected_filter_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterAgentUnderTest);
};

constexpr const char kTestFirstURL[] = "http://example.com/alpha";
constexpr const char kTestSecondURL[] = "http://example.com/beta";
constexpr const char kTestFirstURLPathSuffix[] = "alpha";
constexpr const char kTestSecondURLPathSuffix[] = "beta";
constexpr const char kTestBothURLsPathSuffix[] = "a";

// Histogram names.
constexpr const char kDocumentLoadRulesetIsAvailable[] =
    "SubresourceFilter.DocumentLoad.RulesetIsAvailable";
constexpr const char kDocumentLoadActivationState[] =
    "SubresourceFilter.DocumentLoad.ActivationState";
constexpr const char kSubresourcesEvaluated[] =
    "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Evaluated";
constexpr const char kSubresourcesTotal[] =
    "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Total";
constexpr const char kSubresourcesMatchedRules[] =
    "SubresourceFilter.DocumentLoad.NumSubresourceLoads.MatchedRules";
constexpr const char kSubresourcesDisallowed[] =
    "SubresourceFilter.DocumentLoad.NumSubresourceLoads.Disallowed";
constexpr const char kEvaluationTotalWallDuration[] =
    "SubresourceFilter.DocumentLoad.SubresourceEvaluation.TotalWallDuration";
constexpr const char kEvaluationTotalCPUDuration[] =
    "SubresourceFilter.DocumentLoad.SubresourceEvaluation.TotalCPUDuration";

}  // namespace

class SubresourceFilterAgentTest : public ::testing::Test {
 public:
  SubresourceFilterAgentTest() {}

 protected:
  void SetUp() override { ResetAgent(); }

  void ResetAgent() {
    agent_.reset(new ::testing::StrictMock<SubresourceFilterAgentUnderTest>(
        &ruleset_dealer_));
    ON_CALL(*agent(), GetAncestorDocumentURLs())
        .WillByDefault(::testing::Return(std::vector<GURL>(
            {GURL("http://inner.com/"), GURL("http://outer.com/")})));
  }

  void SetTestRulesetToDisallowURLsWithPathSuffix(base::StringPiece suffix) {
    testing::TestRulesetPair test_ruleset_pair;
    ASSERT_NO_FATAL_FAILURE(
        test_ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
            suffix, &test_ruleset_pair));
    ruleset_dealer_.SetRulesetFile(
        testing::TestRuleset::Open(test_ruleset_pair.indexed));
  }

  void StartLoadWithoutSettingActivationState() {
    agent_as_rfo()->DidStartProvisionalLoad();
    agent_as_rfo()->DidCommitProvisionalLoad(
        true /* is_new_navigation */, false /* is_same_page_navigation */);
  }

  void PerformSamePageNavigationWithoutSettingActivationState() {
    agent_as_rfo()->DidStartProvisionalLoad();
    agent_as_rfo()->DidCommitProvisionalLoad(
        true /* is_new_navigation */, true /* is_same_page_navigation */);
    // No DidFinishLoad is called in this case.
  }

  void StartLoadAndSetActivationState(ActivationState activation_state,
                                      bool measure_performance = false) {
    agent_as_rfo()->DidStartProvisionalLoad();
    EXPECT_TRUE(agent_as_rfo()->OnMessageReceived(
        SubresourceFilterMsg_ActivateForProvisionalLoad(
            0, activation_state, GURL(), measure_performance)));
    agent_as_rfo()->DidCommitProvisionalLoad(
        true /* is_new_navigation */, false /* is_same_page_navigation */);
  }

  void FinishLoad() { agent_as_rfo()->DidFinishLoad(); }

  void ExpectSubresourceFilterGetsInjected() {
    EXPECT_CALL(*agent(), GetAncestorDocumentURLs());
    EXPECT_CALL(*agent(), OnSetSubresourceFilterForCommittedLoadCalled());
  }

  void ExpectNoSubresourceFilterGetsInjected() {
    EXPECT_CALL(*agent(), GetAncestorDocumentURLs())
        .Times(::testing::AtLeast(0));
    EXPECT_CALL(*agent(), OnSetSubresourceFilterForCommittedLoadCalled())
        .Times(0);
  }

  void ExpectSignalAboutFirstSubresourceDisallowed() {
    EXPECT_CALL(*agent(), SignalFirstSubresourceDisallowedForCommittedLoad());
  }

  void ExpectNoSignalAboutFirstSubresourceDisallowed() {
    EXPECT_CALL(*agent(), SignalFirstSubresourceDisallowedForCommittedLoad())
        .Times(0);
  }

  void ExpectLoadAllowed(base::StringPiece url_spec, bool allowed) {
    blink::WebURL url = GURL(url_spec);
    blink::WebURLRequest::RequestContext request_context =
        blink::WebURLRequest::RequestContextImage;
    EXPECT_EQ(allowed, agent()->filter()->allowLoad(url, request_context));
  }

  SubresourceFilterAgentUnderTest* agent() { return agent_.get(); }
  content::RenderFrameObserver* agent_as_rfo() {
    return static_cast<content::RenderFrameObserver*>(agent_.get());
  }

 private:
  testing::TestRulesetCreator test_ruleset_creator_;
  RulesetDealer ruleset_dealer_;

  std::unique_ptr<SubresourceFilterAgentUnderTest> agent_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterAgentTest);
};

TEST_F(SubresourceFilterAgentTest, DisabledByDefault_NoFilterIsInjected) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestBothURLsPathSuffix));
  ExpectNoSubresourceFilterGetsInjected();
  StartLoadWithoutSettingActivationState();
  FinishLoad();

  histogram_tester.ExpectUniqueSample(
      kDocumentLoadActivationState, static_cast<int>(ActivationState::DISABLED),
      1);
  histogram_tester.ExpectTotalCount(kDocumentLoadRulesetIsAvailable, 0);

  histogram_tester.ExpectTotalCount(kSubresourcesTotal, 0);
  histogram_tester.ExpectTotalCount(kSubresourcesEvaluated, 0);
  histogram_tester.ExpectTotalCount(kSubresourcesMatchedRules, 0);
  histogram_tester.ExpectTotalCount(kSubresourcesDisallowed, 0);

  histogram_tester.ExpectTotalCount(kEvaluationTotalWallDuration, 0);
  histogram_tester.ExpectTotalCount(kEvaluationTotalCPUDuration, 0);
}

TEST_F(SubresourceFilterAgentTest, Disabled_NoFilterIsInjected) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestBothURLsPathSuffix));
  ExpectNoSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::DISABLED);
  FinishLoad();
}

TEST_F(SubresourceFilterAgentTest,
       EnabledButRulesetUnavailable_NoFilterIsInjected) {
  base::HistogramTester histogram_tester;
  ExpectNoSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::ENABLED);
  FinishLoad();

  histogram_tester.ExpectUniqueSample(
      kDocumentLoadActivationState, static_cast<int>(ActivationState::ENABLED),
      1);
  histogram_tester.ExpectUniqueSample(kDocumentLoadRulesetIsAvailable, 0, 1);

  histogram_tester.ExpectTotalCount(kSubresourcesTotal, 0);
  histogram_tester.ExpectTotalCount(kSubresourcesEvaluated, 0);
  histogram_tester.ExpectTotalCount(kSubresourcesMatchedRules, 0);
  histogram_tester.ExpectTotalCount(kSubresourcesDisallowed, 0);

  histogram_tester.ExpectTotalCount(kEvaluationTotalWallDuration, 0);
  histogram_tester.ExpectTotalCount(kEvaluationTotalCPUDuration, 0);
}

TEST_F(SubresourceFilterAgentTest, EmptyDocumentLoad_NoFilterIsInjected) {
  base::HistogramTester histogram_tester;
  ExpectNoSubresourceFilterGetsInjected();
  EXPECT_CALL(*agent(), GetAncestorDocumentURLs())
      .WillOnce(::testing::Return(
          std::vector<GURL>({GURL("about:blank"), GURL("http://outer.com/")})));
  StartLoadAndSetActivationState(ActivationState::ENABLED);
  FinishLoad();

  histogram_tester.ExpectTotalCount(kDocumentLoadActivationState, 0);
  histogram_tester.ExpectTotalCount(kDocumentLoadRulesetIsAvailable, 0);

  histogram_tester.ExpectTotalCount(kSubresourcesTotal, 0);
  histogram_tester.ExpectTotalCount(kSubresourcesEvaluated, 0);
  histogram_tester.ExpectTotalCount(kSubresourcesMatchedRules, 0);
  histogram_tester.ExpectTotalCount(kSubresourcesDisallowed, 0);

  histogram_tester.ExpectTotalCount(kEvaluationTotalWallDuration, 0);
  histogram_tester.ExpectTotalCount(kEvaluationTotalCPUDuration, 0);
}

TEST_F(SubresourceFilterAgentTest, Enabled_FilteringIsInEffectForOneLoad) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));

  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::ENABLED);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  ExpectSignalAboutFirstSubresourceDisallowed();
  ExpectLoadAllowed(kTestFirstURL, false);
  ExpectLoadAllowed(kTestSecondURL, true);
  FinishLoad();

  // In-page navigation should not count as a new load.
  ExpectNoSubresourceFilterGetsInjected();
  ExpectNoSignalAboutFirstSubresourceDisallowed();
  PerformSamePageNavigationWithoutSettingActivationState();
  ExpectLoadAllowed(kTestFirstURL, false);
  ExpectLoadAllowed(kTestSecondURL, true);

  ExpectNoSubresourceFilterGetsInjected();
  StartLoadWithoutSettingActivationState();
  FinishLoad();

  // Resource loads after the in-page navigation should not be counted toward
  // the figures below, as they came after the original page load event.
  histogram_tester.ExpectUniqueSample(kSubresourcesTotal, 2, 1);
  histogram_tester.ExpectUniqueSample(kSubresourcesEvaluated, 2, 1);
  histogram_tester.ExpectUniqueSample(kSubresourcesMatchedRules, 1, 1);
  histogram_tester.ExpectUniqueSample(kSubresourcesDisallowed, 1, 1);
  EXPECT_THAT(histogram_tester.GetAllSamples(kDocumentLoadActivationState),
              ::testing::ElementsAre(
                  base::Bucket(static_cast<int>(ActivationState::DISABLED), 1),
                  base::Bucket(static_cast<int>(ActivationState::ENABLED), 1)));
  histogram_tester.ExpectUniqueSample(kDocumentLoadRulesetIsAvailable, 1, 1);
}

TEST_F(SubresourceFilterAgentTest, Enabled_HistogramSamplesOverTwoLoads) {
  for (const bool measure_performance : {false, true}) {
    base::HistogramTester histogram_tester;
    ASSERT_NO_FATAL_FAILURE(
        SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
    ExpectSubresourceFilterGetsInjected();
    StartLoadAndSetActivationState(ActivationState::ENABLED,
                                   measure_performance);
    ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

    ExpectSignalAboutFirstSubresourceDisallowed();
    ExpectLoadAllowed(kTestFirstURL, false);
    ExpectNoSignalAboutFirstSubresourceDisallowed();
    ExpectLoadAllowed(kTestFirstURL, false);
    ExpectNoSignalAboutFirstSubresourceDisallowed();
    ExpectLoadAllowed(kTestSecondURL, true);
    EXPECT_CALL(*agent(),
                SendDocumentLoadStatistics(::testing::_, ::testing::_))
        .Times(measure_performance && ScopedThreadTimers::IsSupported());
    FinishLoad();

    ExpectSubresourceFilterGetsInjected();
    StartLoadAndSetActivationState(ActivationState::ENABLED,
                                   measure_performance);
    ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

    ExpectNoSignalAboutFirstSubresourceDisallowed();
    ExpectLoadAllowed(kTestSecondURL, true);
    ExpectSignalAboutFirstSubresourceDisallowed();
    ExpectLoadAllowed(kTestFirstURL, false);
    EXPECT_CALL(*agent(),
                SendDocumentLoadStatistics(::testing::_, ::testing::_))
        .Times(measure_performance && ScopedThreadTimers::IsSupported());
    FinishLoad();

    histogram_tester.ExpectUniqueSample(
        kDocumentLoadActivationState,
        static_cast<int>(ActivationState::ENABLED), 2);
    histogram_tester.ExpectUniqueSample(kDocumentLoadRulesetIsAvailable, 1, 2);

    EXPECT_THAT(histogram_tester.GetAllSamples(kSubresourcesTotal),
                ::testing::ElementsAre(base::Bucket(2, 1), base::Bucket(3, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(kSubresourcesEvaluated),
                ::testing::ElementsAre(base::Bucket(2, 1), base::Bucket(3, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(kSubresourcesMatchedRules),
                ::testing::ElementsAre(base::Bucket(1, 1), base::Bucket(2, 1)));
    EXPECT_THAT(histogram_tester.GetAllSamples(kSubresourcesDisallowed),
                ::testing::ElementsAre(base::Bucket(1, 1), base::Bucket(2, 1)));

    base::HistogramBase::Count expected_total_count =
        measure_performance && base::ThreadTicks::IsSupported() ? 2 : 0;
    histogram_tester.ExpectTotalCount(kEvaluationTotalWallDuration,
                                      expected_total_count);
    histogram_tester.ExpectTotalCount(kEvaluationTotalCPUDuration,
                                      expected_total_count);
  }
}

TEST_F(SubresourceFilterAgentTest, Enabled_NewRulesetIsPickedUpAtNextLoad) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::ENABLED);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  // Set the new ruleset just after the deadline for being used for the current
  // load, to exercises doing filtering based on obseleted rulesets.
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestSecondURLPathSuffix));

  ExpectSignalAboutFirstSubresourceDisallowed();
  ExpectLoadAllowed(kTestFirstURL, false);
  ExpectLoadAllowed(kTestSecondURL, true);
  FinishLoad();

  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::ENABLED);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  ExpectSignalAboutFirstSubresourceDisallowed();
  ExpectLoadAllowed(kTestFirstURL, true);
  ExpectLoadAllowed(kTestSecondURL, false);
  FinishLoad();
}

// If a provisional load is aborted, the RenderFrameObservers might not receive
// any further notifications about that load. It is thus possible that there
// will be two RenderFrameObserver::DidStartProvisionalLoad in a row. Make sure
// that the activation decision does not outlive the first provisional load.
TEST_F(SubresourceFilterAgentTest,
       Enabled_FilteringNoLongerEffectAfterProvisionalLoadIsCancelled) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestBothURLsPathSuffix));
  ExpectNoSubresourceFilterGetsInjected();
  agent_as_rfo()->DidStartProvisionalLoad();
  EXPECT_TRUE(agent_as_rfo()->OnMessageReceived(
      SubresourceFilterMsg_ActivateForProvisionalLoad(
          0, ActivationState::ENABLED, GURL(), true)));
  agent_as_rfo()->DidStartProvisionalLoad();
  agent_as_rfo()->DidCommitProvisionalLoad(true /* is_new_navigation */,
                                           false /* is_same_page_navigation */);
  FinishLoad();
}

TEST_F(SubresourceFilterAgentTest, DryRun_ResourcesAreEvaluatedButNotFiltered) {
  base::HistogramTester histogram_tester;
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::DRYRUN);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  // In dry-run mode, loads to the first URL should be recorded as
  // `MatchedRules`, but still be allowed to proceed and not recorded as
  // `Disallowed`.
  ExpectLoadAllowed(kTestFirstURL, true);
  ExpectLoadAllowed(kTestFirstURL, true);
  ExpectLoadAllowed(kTestSecondURL, true);
  FinishLoad();

  histogram_tester.ExpectUniqueSample(kDocumentLoadActivationState,
                                      static_cast<int>(ActivationState::DRYRUN),
                                      1);
  histogram_tester.ExpectUniqueSample(kDocumentLoadRulesetIsAvailable, 1, 1);

  histogram_tester.ExpectUniqueSample(kSubresourcesTotal, 3, 1);
  histogram_tester.ExpectUniqueSample(kSubresourcesEvaluated, 3, 1);
  histogram_tester.ExpectUniqueSample(kSubresourcesMatchedRules, 2, 1);
  histogram_tester.ExpectUniqueSample(kSubresourcesDisallowed, 0, 1);

  // Performance measurement is switched off.
  histogram_tester.ExpectTotalCount(kEvaluationTotalWallDuration, 0);
  histogram_tester.ExpectTotalCount(kEvaluationTotalCPUDuration, 0);
}

TEST_F(SubresourceFilterAgentTest,
       SignalFirstSubresourceDisallowed_OncePerDocumentLoad) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::ENABLED);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  ExpectSignalAboutFirstSubresourceDisallowed();
  ExpectLoadAllowed(kTestFirstURL, false);
  ExpectNoSignalAboutFirstSubresourceDisallowed();
  ExpectLoadAllowed(kTestFirstURL, false);
  ExpectLoadAllowed(kTestSecondURL, true);
  FinishLoad();

  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::ENABLED);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  ExpectLoadAllowed(kTestSecondURL, true);
  ExpectSignalAboutFirstSubresourceDisallowed();
  ExpectLoadAllowed(kTestFirstURL, false);
  FinishLoad();
}

TEST_F(SubresourceFilterAgentTest,
       SignalFirstSubresourceDisallowed_ComesAfterAgentDestroyed) {
  ASSERT_NO_FATAL_FAILURE(
      SetTestRulesetToDisallowURLsWithPathSuffix(kTestFirstURLPathSuffix));
  ExpectSubresourceFilterGetsInjected();
  StartLoadAndSetActivationState(ActivationState::ENABLED);
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(agent()));

  auto filter = agent()->TakeFilter();
  ResetAgent();
  EXPECT_FALSE(filter->allowLoad(GURL(kTestFirstURL),
                                 blink::WebURLRequest::RequestContextImage));
}

}  // namespace subresource_filter
