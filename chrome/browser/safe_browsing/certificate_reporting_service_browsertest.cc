// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/certificate_reporting_service.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/thread_test_helper.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_factory.h"
#include "chrome/browser/safe_browsing/certificate_reporting_service_test_utils.h"
#include "chrome/browser/ssl/certificate_reporting_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/certificate_reporting/error_report.h"
#include "components/prefs/pref_service.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/report_sender.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_util.h"
#include "url/scheme_host_port.h"

using certificate_reporting_test_utils::CertificateReportingServiceTestHelper;
using certificate_reporting_test_utils::CertificateReportingServiceObserver;
using certificate_reporting_test_utils::ReportExpectation;

namespace {

const char* kFailedReportHistogram = "SSL.CertificateErrorReportFailure";

void CleanUpOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  net::URLRequestFilter::GetInstance()->ClearHandlers();
}

}  // namespace

namespace safe_browsing {

// These tests check the whole mechanism to send and queue invalid certificate
// reports. Each test triggers reports by visiting broken SSL pages. The reports
// succeed, fail or hang indefinitely:
// - If a report is expected to fail or succeed, the test waits for the
//   corresponding URL request jobs to be destroyed.
// - If a report is expected to hang, the test waits for the corresponding URL
//   request job to be created. Only after resuming the hung request job the
//   test waits for the request to be destroyed.
class CertificateReportingServiceBrowserTest : public InProcessBrowserTest {
 public:
  CertificateReportingServiceBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
    https_server_.ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(https_server_.Start());

    test_helper()->SetUpInterceptor();

    CertificateReportingServiceFactory::GetInstance()
        ->SetReportEncryptionParamsForTesting(
            test_helper()->server_public_key(),
            test_helper()->server_public_key_version());
    CertificateReportingServiceFactory::GetInstance()
        ->SetServiceResetCallbackForTesting(
            base::Bind(&CertificateReportingServiceObserver::OnServiceReset,
                       base::Unretained(&service_observer_)));
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    test_helper()->ExpectNoRequests(service());
    content::BrowserThread::PostTask(content::BrowserThread::IO, FROM_HERE,
                                     base::Bind(&CleanUpOnIOThread));
    EXPECT_GE(num_expected_failed_report_, 0)
        << "Don't forget to set expected failed report count.";
    // Check the histogram as the last thing. This makes sure no in-flight
    // report is missed.
    if (num_expected_failed_report_ != 0) {
      histogram_tester_.ExpectUniqueSample(kFailedReportHistogram,
                                           -net::ERR_SSL_PROTOCOL_ERROR,
                                           num_expected_failed_report_);
    } else {
      histogram_tester_.ExpectTotalCount(kFailedReportHistogram, 0);
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        switches::kForceFieldTrials,
        "ReportCertificateErrors/ShowAndPossiblySend/");
    // Setting the sending threshold to 1.0 ensures reporting is enabled.
    command_line->AppendSwitchASCII(
        variations::switches::kForceFieldTrialParams,
        "ReportCertificateErrors.ShowAndPossiblySend:sendingThreshold/1.0");
  }

  CertificateReportingServiceTestHelper* test_helper() { return &test_helper_; }

 protected:
  CertificateReportingServiceFactory* factory() {
    return CertificateReportingServiceFactory::GetInstance();
  }

  // Sends a report using the provided hostname. Navigates to an interstitial
  // page on this hostname and away from it to trigger a report.
  void SendReport(const std::string& hostname) {
    // Create an HTTPS URL from the hostname. This will resolve to the HTTPS
    // server and cause an SSL error.
    const GURL kCertErrorURL(
        url::SchemeHostPort("https", hostname, https_server_.port()).GetURL());

    // Navigate to the page with SSL error.
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    content::WebContents* contents = tab_strip_model->GetActiveWebContents();
    ui_test_utils::NavigateToURL(browser(), kCertErrorURL);
    content::WaitForInterstitialAttach(contents);

    // Navigate away from the interstitial to trigger report upload.
    ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
    content::WaitForInterstitialDetach(contents);
  }

  void SendPendingReports() { service()->SendPending(); }

  // Changes opt-in status and waits for the cert reporting service to reset.
  // Can only be used after the service is initialized. When changing the
  // value at the beginning of a test,
  // certificate_reporting_test_utils::SetCertReportingOptIn should be used
  // instead since the service is only created upon first SSL error.
  // Changing the opt-in status synchronously fires
  // CertificateReportingService::PreferenceObserver::OnPreferenceChanged which
  // will call CertificateReportingService::SetEnabled() which in turn posts
  // a task to the IO thread to reset the service. Waiting for the IO thread
  // ensures that the service is reset before returning from this method.
  void ChangeOptInAndWait(certificate_reporting_test_utils::OptIn opt_in) {
    service_observer_.Clear();
    certificate_reporting_test_utils::SetCertReportingOptIn(browser(), opt_in);
    service_observer_.WaitForReset();
  }

  // Same as ChangeOptInAndWait, but enables/disables SafeBrowsing instead.
  void ToggleSafeBrowsingAndWaitForServiceReset(bool safebrowsing_enabled) {
    service_observer_.Clear();
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled,
                                                 safebrowsing_enabled);
    service_observer_.WaitForReset();
  }

  void SetExpectedHistogramCountOnTeardown(int num_expected_failed_report) {
    num_expected_failed_report_ = num_expected_failed_report;
  }

  CertificateReportingService* service() const {
    return CertificateReportingServiceFactory::GetForBrowserContext(
        browser()->profile());
  }

 private:
  // Checks that the serialized reports in |received_reports| have the same
  // hostnames as |expected_hostnames|.
  void CheckReports(const std::set<std::string>& expected_hostnames,
                    const std::set<std::string>& received_reports,
                    const std::string type) {
    std::set<std::string> received_hostnames;
    for (const std::string& serialized_report : received_reports) {
      certificate_reporting::ErrorReport report;
      ASSERT_TRUE(report.InitializeFromString(serialized_report));
      received_hostnames.insert(report.hostname());
    }
    EXPECT_EQ(expected_hostnames, received_hostnames) << type
                                                      << " comparison failed";
  }

  net::EmbeddedTestServer https_server_;

  int num_expected_failed_report_ = -1;

  CertificateReportingServiceTestHelper test_helper_;

  CertificateReportingServiceObserver service_observer_;

  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(CertificateReportingServiceBrowserTest);
};

// Tests that report send attempt should be cancelled when extended
// reporting is not opted in.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       NotOptedIn_ShouldNotSendReports) {
  SetExpectedHistogramCountOnTeardown(0);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(),
      certificate_reporting_test_utils::EXTENDED_REPORTING_DO_NOT_OPT_IN);
  // Send a report. Test teardown checks for created and in-flight requests. If
  // a report was incorrectly sent, the test will fail.
  SendReport("no-report");
}

// Tests that report send attempts are not cancelled when extended reporting is
// opted in. Goes to an interstitial page and navigates away to force a report
// send event.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       OptedIn_ShouldSendSuccessfulReport) {
  SetExpectedHistogramCountOnTeardown(0);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);

  // Let reports uploads successfully complete.
  test_helper()->SetFailureMode(certificate_reporting_test_utils::
                                    ReportSendingResult::REPORTS_SUCCESSFUL);

  // Reporting is opted in, so the report should succeed.
  SendReport("report0");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({"report0"}));
}

// Tests that report send attempts are not cancelled when extended reporting is
// opted in. Goes to an interstitial page and navigate away to force a report
// send event. Repeats this three times and checks expected number of reports.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       OptedIn_ShouldQueueFailedReport) {
  SetExpectedHistogramCountOnTeardown(2);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);
  // Let all reports fail.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  // Send a failed report.
  SendReport("report0");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({"report0"}));

  // Send another failed report.
  SendReport("report1");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({"report1"}));

  // Let all report uploads complete successfully now.
  test_helper()->SetFailureMode(certificate_reporting_test_utils::
                                    ReportSendingResult::REPORTS_SUCCESSFUL);

  // Send another report. This time the report should be successfully sent.
  SendReport("report2");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({"report2"}));

  // Send all pending reports. The two previously failed reports should have
  // been queued, and now be sent successfully.
  SendPendingReports();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({"report0", "report1"}));

  // Try sending pending reports again. Since there is no pending report,
  // nothing should be sent this time. If any report is sent, test teardown
  // will catch it.
  SendPendingReports();
}

// Opting in then opting out of extended reporting should clear the pending
// report queue.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       OptedIn_ThenOptedOut) {
  SetExpectedHistogramCountOnTeardown(1);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);
  // Let all reports fail.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  // Send a failed report.
  SendReport("report0");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({"report0"}));

  // Disable reporting. This should clear all pending reports.
  ChangeOptInAndWait(
      certificate_reporting_test_utils::EXTENDED_REPORTING_DO_NOT_OPT_IN);

  // Send pending reports. No reports should be observed during test teardown.
  SendPendingReports();
}

// Opting out, then in, then out of extended reporting should work as expected.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       OptedOut_ThenOptedIn_ThenOptedOut) {
  SetExpectedHistogramCountOnTeardown(1);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(),
      certificate_reporting_test_utils::EXTENDED_REPORTING_DO_NOT_OPT_IN);
  // Let all reports fail.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  // Send attempt should be cancelled since reporting is opted out.
  SendReport("no-report");
  test_helper()->ExpectNoRequests(service());

  // Enable reporting.
  ChangeOptInAndWait(
      certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);

  // A failed report should be observed.
  SendReport("report0");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({"report0"}));

  // Disable reporting. This should reset the reporting service and
  // clear all pending reports.
  ChangeOptInAndWait(
      certificate_reporting_test_utils::EXTENDED_REPORTING_DO_NOT_OPT_IN);

  // Report should be cancelled since reporting is opted out.
  SendReport("report1");
  test_helper()->ExpectNoRequests(service());

  // Send pending reports. Nothing should be sent since there aren't any
  // pending reports. If any report is sent, test teardown will catch it.
  SendPendingReports();
}

// Disabling SafeBrowsing should clear pending reports queue in
// CertificateReportingService.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       DisableSafebrowsing) {
  SetExpectedHistogramCountOnTeardown(2);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);
  // Let all reports fail.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  // Send a delayed report.
  SendReport("report0");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({"report0"}));

  // Disable SafeBrowsing. This should clear all pending reports.
  ToggleSafeBrowsingAndWaitForServiceReset(false);

  // Send pending reports. No reports should be observed.
  SendPendingReports();
  test_helper()->ExpectNoRequests(service());

  // Re-enable SafeBrowsing and trigger another report which will be queued.
  ToggleSafeBrowsingAndWaitForServiceReset(true);
  SendReport("report1");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({"report1"}));

  // Queued report should now be successfully sent.
  test_helper()->SetFailureMode(certificate_reporting_test_utils::
                                    ReportSendingResult::REPORTS_SUCCESSFUL);
  SendPendingReports();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({"report1"}));
}

// CertificateReportingService should ignore reports older than the report TTL.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       DontSendOldReports) {
  SetExpectedHistogramCountOnTeardown(5);

  base::SimpleTestClock* clock = new base::SimpleTestClock();
  base::Time reference_time = base::Time::Now();
  clock->SetNow(reference_time);
  factory()->SetClockForTesting(std::unique_ptr<base::Clock>(clock));

  // The service should ignore reports older than 24 hours.
  factory()->SetQueuedReportTTLForTesting(base::TimeDelta::FromHours(24));

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);

  // Let all reports fail.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  // Send a failed report.
  SendReport("report0");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({"report0"}));

  // Advance the clock a bit and trigger another failed report.
  clock->Advance(base::TimeDelta::FromHours(5));
  SendReport("report1");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({"report1"}));

  // Advance the clock to 20 hours, putting it 25 hours ahead of the reference
  // time. This makes report0 older than 24 hours. report1 is now 20 hours.
  clock->Advance(base::TimeDelta::FromHours(20));

  // Send pending reports. report0 should be discarded since it's too old.
  // report1 should be queued again.
  SendPendingReports();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({"report1"}));

  // Trigger another failed report.
  SendReport("report2");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({"report2"}));

  // Advance the clock 5 hours. report1 will now be 25 hours old.
  clock->Advance(base::TimeDelta::FromHours(5));

  // Send pending reports. report1 should be discarded since it's too old.
  // report2 should be queued again.
  SendPendingReports();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({"report2"}));

  // Advance the clock 20 hours again so that report2 is 25 hours old and is
  // older than max age (24 hours).
  clock->Advance(base::TimeDelta::FromHours(20));

  // Send pending reports. report2 should be discarded since it's too old. No
  // other reports remain. If any report is sent, test teardown will catch it.
  SendPendingReports();
}

// CertificateReportingService should drop old reports from its pending report
// queue, if the queue is full.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       DropOldReportsFromQueue) {
  SetExpectedHistogramCountOnTeardown(7);

  base::SimpleTestClock* clock = new base::SimpleTestClock();
  base::Time reference_time = base::Time::Now();
  clock->SetNow(reference_time);
  factory()->SetClockForTesting(std::unique_ptr<base::Clock>(clock));

  // The service should queue a maximum of 3 reports and ignore reports older
  // than 24 hours.
  factory()->SetQueuedReportTTLForTesting(base::TimeDelta::FromHours(24));
  factory()->SetMaxQueuedReportCountForTesting(3);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);

  // Let all reports fail.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_FAIL);

  // Trigger a failed report.
  SendReport("report0");
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({"report0"}));

  // Trigger three more reports within five hours of each other. After this:
  // report0 is 0 hours after reference time (15 hours old).
  // report1 is 5 hours after reference time (10 hours old).
  // report2 is 10 hours after reference time (5 hours old).
  // report3 is 15 hours after reference time (0 hours old).
  clock->Advance(base::TimeDelta::FromHours(5));
  SendReport("report1");

  clock->Advance(base::TimeDelta::FromHours(5));
  SendReport("report2");

  clock->Advance(base::TimeDelta::FromHours(5));
  SendReport("report3");

  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({"report1", "report2", "report3"}));

  // Send pending reports. Four reports were generated above, but the service
  // only queues three reports, so report0 should be dropped since it's the
  // oldest.
  SendPendingReports();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Failed({"report1", "report2", "report3"}));

  // Let all reports succeed.
  test_helper()->SetFailureMode(certificate_reporting_test_utils::
                                    ReportSendingResult::REPORTS_SUCCESSFUL);

  // Advance the clock 15 hours. Current time is now 30 hours after reference
  // time, and the ages of reports are now as follows:
  // report1 is 25 hours old.
  // report2 is 20 hours old.
  // report3 is 15 hours old.
  clock->Advance(base::TimeDelta::FromHours(15));

  // Send pending reports. Only reports 2 and 3 should be sent, report 1
  // should be ignored because it's too old.
  SendPendingReports();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Successful({"report2", "report3"}));
}

IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       Delayed_Resumed) {
  SetExpectedHistogramCountOnTeardown(0);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);
  // Let all reports hang.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_DELAY);

  // Trigger a report that hangs.
  SendReport("report0");
  test_helper()->WaitForRequestsCreated(
      ReportExpectation::Delayed({"report0"}));

  // Resume the report upload. The report upload should successfully complete.
  // The interceptor only observes request creations and not response
  // completions, so there is nothing to observe.
  test_helper()->ResumeDelayedRequest();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Delayed({"report0"}));
}

// Same as above, but the service is shut down before resuming the delayed
// request. Should not crash.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest,
                       Delayed_Resumed_ServiceShutdown) {
  SetExpectedHistogramCountOnTeardown(0);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);
  // Let all reports hang.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_DELAY);

  // Trigger a report that hangs.
  SendReport("report0");
  test_helper()->WaitForRequestsCreated(
      ReportExpectation::Delayed({"report0"}));

  // Shutdown the service and resume the report upload. Shouldn't crash.
  service()->Shutdown();
  test_helper()->ResumeDelayedRequest();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Delayed({"report0"}));
}

// Trigger a delayed report, then disable Safebrowsing. Certificate reporting
// service should clear its in-flight reports list.
IN_PROC_BROWSER_TEST_F(CertificateReportingServiceBrowserTest, Delayed_Reset) {
  SetExpectedHistogramCountOnTeardown(0);

  certificate_reporting_test_utils::SetCertReportingOptIn(
      browser(), certificate_reporting_test_utils::EXTENDED_REPORTING_OPT_IN);
  // Let all reports hang.
  test_helper()->SetFailureMode(
      certificate_reporting_test_utils::ReportSendingResult::REPORTS_DELAY);

  // Trigger a report that hangs.
  SendReport("report0");
  test_helper()->WaitForRequestsCreated(
      ReportExpectation::Delayed({"report0"}));

  // Disable SafeBrowsing. This should clear all pending reports.
  ToggleSafeBrowsingAndWaitForServiceReset(false);
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Delayed({"report0"}));

  // Resume delayed report. No response should be observed since all pending
  // reports should be cleared.
  test_helper()->ResumeDelayedRequest();
  test_helper()->ExpectNoRequests(service());

  // Re-enable SafeBrowsing.
  ToggleSafeBrowsingAndWaitForServiceReset(true);

  // Trigger a report that hangs.
  SendReport("report1");
  test_helper()->WaitForRequestsCreated(
      ReportExpectation::Delayed({"report1"}));

  // Resume the delayed report and wait for it to complete.
  test_helper()->ResumeDelayedRequest();
  test_helper()->WaitForRequestsDestroyed(
      ReportExpectation::Delayed({"report1"}));
}

}  // namespace safe_browsing
