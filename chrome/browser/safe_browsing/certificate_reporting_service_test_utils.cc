// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/certificate_reporting_service_test_utils.h"

#include "base/threading/thread_task_runner_handle.h"
#include "components/certificate_reporting/encrypted_cert_logger.pb.h"
#include "components/certificate_reporting/error_report.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "crypto/curve25519.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/url_request/url_request_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const uint32_t kServerPublicKeyTestVersion = 16;

void SetUpURLHandlersOnIOThread(
    std::unique_ptr<net::URLRequestInterceptor> url_request_interceptor) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  net::URLRequestFilter* filter = net::URLRequestFilter::GetInstance();
  filter->AddUrlInterceptor(
      CertificateReportingService::GetReportingURLForTesting(),
      std::move(url_request_interceptor));
}

std::string GetUploadData(net::URLRequest* request) {
  const net::UploadDataStream* stream = request->get_upload();
  EXPECT_TRUE(stream);
  EXPECT_TRUE(stream->GetElementReaders());
  EXPECT_EQ(1u, stream->GetElementReaders()->size());
  const net::UploadBytesElementReader* reader =
      (*stream->GetElementReaders())[0]->AsBytesReader();
  return std::string(reader->bytes(), reader->length());
}

std::string GetReportContents(net::URLRequest* request,
                              const uint8_t* server_private_key) {
  std::string serialized_report(GetUploadData(request));
  certificate_reporting::EncryptedCertLoggerRequest encrypted_request;
  EXPECT_TRUE(encrypted_request.ParseFromString(serialized_report));
  EXPECT_EQ(kServerPublicKeyTestVersion,
            encrypted_request.server_public_key_version());
  EXPECT_EQ(certificate_reporting::EncryptedCertLoggerRequest::
                AEAD_ECDH_AES_128_CTR_HMAC_SHA256,
            encrypted_request.algorithm());
  std::string decrypted_report;
  certificate_reporting::ErrorReporter::DecryptErrorReport(
      server_private_key, encrypted_request, &decrypted_report);
  return decrypted_report;
}

// Checks that the serialized reports in |observed_reports| have the same
// hostnames as |expected_hostnames|.
void CompareHostnames(const std::set<std::string>& expected_hostnames,
                      const std::set<std::string>& observed_reports,
                      const std::string& comparison_type) {
  std::set<std::string> observed_hostnames;
  for (const std::string& serialized_report : observed_reports) {
    certificate_reporting::ErrorReport report;
    ASSERT_TRUE(report.InitializeFromString(serialized_report));
    observed_hostnames.insert(report.hostname());
  }
  EXPECT_EQ(expected_hostnames, observed_hostnames)
      << "Comparison failed for " << comparison_type << " reports.";
}

void WaitReports(
    certificate_reporting_test_utils::RequestObserver* observer,
    const certificate_reporting_test_utils::ReportExpectation& expectation) {
  observer->Wait(expectation.num_reports());
  CompareHostnames(expectation.successful_reports,
                   observer->successful_reports(), "successful");
  CompareHostnames(expectation.failed_reports, observer->failed_reports(),
                   "failed");
  CompareHostnames(expectation.delayed_reports, observer->delayed_reports(),
                   "delayed");
  observer->ClearObservedReports();
}

}  // namespace

namespace certificate_reporting_test_utils {

RequestObserver::RequestObserver()
    : num_events_to_wait_for_(0u), num_received_events_(0u) {}
RequestObserver::~RequestObserver() {}

void RequestObserver::Wait(unsigned int num_events_to_wait_for) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!run_loop_);
  ASSERT_LE(num_received_events_, num_events_to_wait_for)
      << "Observed unexpected report";

  if (num_received_events_ < num_events_to_wait_for) {
    num_events_to_wait_for_ = num_events_to_wait_for;
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
    run_loop_.reset(nullptr);
    EXPECT_EQ(0u, num_received_events_);
    EXPECT_EQ(0u, num_events_to_wait_for_);
  } else if (num_received_events_ == num_events_to_wait_for) {
    num_received_events_ = 0u;
    num_events_to_wait_for_ = 0u;
  }
}

void RequestObserver::OnRequest(const std::string& serialized_report,
                                ReportSendingResult report_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  switch (report_type) {
    case REPORTS_SUCCESSFUL:
      successful_reports_.insert(serialized_report);
      break;
    case REPORTS_FAIL:
      failed_reports_.insert(serialized_report);
      break;
    case REPORTS_DELAY:
      delayed_reports_.insert(serialized_report);
      break;
  }

  num_received_events_++;
  if (!run_loop_) {
    return;
  }
  ASSERT_LE(num_received_events_, num_events_to_wait_for_)
      << "Observed unexpected report";

  if (num_received_events_ == num_events_to_wait_for_) {
    num_events_to_wait_for_ = 0u;
    num_received_events_ = 0u;
    run_loop_->Quit();
  }
}

const std::set<std::string>& RequestObserver::successful_reports() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return successful_reports_;
}

const std::set<std::string>& RequestObserver::failed_reports() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return failed_reports_;
}

const std::set<std::string>& RequestObserver::delayed_reports() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return delayed_reports_;
}

void RequestObserver::ClearObservedReports() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  successful_reports_.clear();
  failed_reports_.clear();
  delayed_reports_.clear();
}

DelayableCertReportURLRequestJob::DelayableCertReportURLRequestJob(
    bool delayed,
    bool should_fail,
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const base::Callback<void()>& destruction_callback)
    : net::URLRequestJob(request, network_delegate),
      delayed_(delayed),
      should_fail_(should_fail),
      started_(false),
      destruction_callback_(destruction_callback),
      weak_factory_(this) {}

DelayableCertReportURLRequestJob::~DelayableCertReportURLRequestJob() {
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   destruction_callback_);
}

base::WeakPtr<DelayableCertReportURLRequestJob>
DelayableCertReportURLRequestJob::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void DelayableCertReportURLRequestJob::Start() {
  started_ = true;
  if (delayed_) {
    // Do nothing until Resume() is called.
    return;
  }
  Resume();
}

int DelayableCertReportURLRequestJob::ReadRawData(net::IOBuffer* buf,
                                                  int buf_size) {
  // Report sender ignores responses. Return empty response.
  return 0;
}

int DelayableCertReportURLRequestJob::GetResponseCode() const {
  // Report sender ignores responses. Return empty response.
  return 200;
}

void DelayableCertReportURLRequestJob::GetResponseInfo(
    net::HttpResponseInfo* info) {
  // Report sender ignores responses. Return empty response.
}

void DelayableCertReportURLRequestJob::Resume() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!started_) {
    // If Start() hasn't been called yet, then unset |delayed_| so that when
    // Start() is called, the request will begin immediately.
    delayed_ = false;
    return;
  }
  if (should_fail_) {
    NotifyStartError(net::URLRequestStatus(net::URLRequestStatus::FAILED,
                                           net::ERR_SSL_PROTOCOL_ERROR));
    return;
  }
  // Start reading asynchronously as would a normal network request.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&DelayableCertReportURLRequestJob::NotifyHeadersComplete,
                 weak_factory_.GetWeakPtr()));
}

CertReportJobInterceptor::CertReportJobInterceptor(
    ReportSendingResult expected_report_result,
    const uint8_t* server_private_key)
    : expected_report_result_(expected_report_result),
      server_private_key_(server_private_key),
      weak_factory_(this) {}

CertReportJobInterceptor::~CertReportJobInterceptor() {}

net::URLRequestJob* CertReportJobInterceptor::MaybeInterceptRequest(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  const std::string uploaded_report =
      GetReportContents(request, server_private_key_);
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(&CertReportJobInterceptor::RequestCreated,
                 weak_factory_.GetWeakPtr(), uploaded_report,
                 expected_report_result_));

  if (expected_report_result_ == REPORTS_FAIL) {
    return new DelayableCertReportURLRequestJob(
        false, true, request, network_delegate,
        base::Bind(&CertReportJobInterceptor::RequestDestructed,
                   base::Unretained(this), uploaded_report,
                   expected_report_result_));

  } else if (expected_report_result_ == REPORTS_DELAY) {
    DCHECK(!delayed_request_) << "Supports only one delayed request at a time";
    DelayableCertReportURLRequestJob* job =
        new DelayableCertReportURLRequestJob(
            true, false, request, network_delegate,
            base::Bind(&CertReportJobInterceptor::RequestDestructed,
                       base::Unretained(this), uploaded_report,
                       expected_report_result_));
    delayed_request_ = job->GetWeakPtr();
    return job;
  }
  // Successful url request job.
  return new DelayableCertReportURLRequestJob(
      false, false, request, network_delegate,
      base::Bind(&CertReportJobInterceptor::RequestDestructed,
                 base::Unretained(this), uploaded_report,
                 expected_report_result_));
}

void CertReportJobInterceptor::SetFailureMode(
    ReportSendingResult expected_report_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&CertReportJobInterceptor::SetFailureModeOnIOThread,
                 weak_factory_.GetWeakPtr(), expected_report_result));
}

void CertReportJobInterceptor::Resume() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&CertReportJobInterceptor::ResumeOnIOThread,
                 base::Unretained(this)));
}

RequestObserver* CertReportJobInterceptor::request_created_observer() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return &request_created_observer_;
}

RequestObserver* CertReportJobInterceptor::request_destroyed_observer() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return &request_destroyed_observer_;
}

void CertReportJobInterceptor::SetFailureModeOnIOThread(
    ReportSendingResult expected_report_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  expected_report_result_ = expected_report_result;
}

void CertReportJobInterceptor::ResumeOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  EXPECT_EQ(REPORTS_DELAY, expected_report_result_);
  if (delayed_request_)
    delayed_request_->Resume();
}

void CertReportJobInterceptor::RequestCreated(
    const std::string& uploaded_report,
    ReportSendingResult expected_report_result) const {
  request_created_observer_.OnRequest(uploaded_report, expected_report_result);
}

void CertReportJobInterceptor::RequestDestructed(
    const std::string& uploaded_report,
    ReportSendingResult expected_report_result) const {
  request_destroyed_observer_.OnRequest(uploaded_report,
                                        expected_report_result);
}

ReportExpectation::ReportExpectation() {}

ReportExpectation::ReportExpectation(const ReportExpectation& other) = default;

ReportExpectation::~ReportExpectation() {}

// static
ReportExpectation ReportExpectation::Successful(
    const std::set<std::string>& reports) {
  ReportExpectation expectation;
  expectation.successful_reports = reports;
  return expectation;
}

// static
ReportExpectation ReportExpectation::Failed(
    const std::set<std::string>& reports) {
  ReportExpectation expectation;
  expectation.failed_reports = reports;
  return expectation;
}

// static
ReportExpectation ReportExpectation::Delayed(
    const std::set<std::string>& reports) {
  ReportExpectation expectation;
  expectation.delayed_reports = reports;
  return expectation;
}

int ReportExpectation::num_reports() const {
  return successful_reports.size() + failed_reports.size() +
         delayed_reports.size();
}

CertificateReportingServiceObserver::CertificateReportingServiceObserver() {}

CertificateReportingServiceObserver::~CertificateReportingServiceObserver() {}

void CertificateReportingServiceObserver::Clear() {
  did_reset_ = false;
}

void CertificateReportingServiceObserver::WaitForReset() {
  DCHECK(!run_loop_);
  if (did_reset_)
    return;
  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();
  run_loop_.reset();
}

void CertificateReportingServiceObserver::OnServiceReset() {
  did_reset_ = true;
  if (run_loop_)
    run_loop_->Quit();
}

CertificateReportingServiceTestHelper::CertificateReportingServiceTestHelper() {
  memset(server_private_key_, 1, sizeof(server_private_key_));
  crypto::curve25519::ScalarBaseMult(server_private_key_, server_public_key_);
}

CertificateReportingServiceTestHelper::
    ~CertificateReportingServiceTestHelper() {}

void CertificateReportingServiceTestHelper::SetUpInterceptor() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  url_request_interceptor_ =
      new CertReportJobInterceptor(REPORTS_FAIL, server_private_key_);
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&SetUpURLHandlersOnIOThread,
                 base::Passed(std::unique_ptr<net::URLRequestInterceptor>(
                     url_request_interceptor_))));
}

void CertificateReportingServiceTestHelper::SetFailureMode(
    ReportSendingResult expected_report_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  url_request_interceptor_->SetFailureMode(expected_report_result);
}

void CertificateReportingServiceTestHelper::ResumeDelayedRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  url_request_interceptor_->Resume();
}

uint8_t* CertificateReportingServiceTestHelper::server_public_key() {
  return server_public_key_;
}

uint32_t CertificateReportingServiceTestHelper::server_public_key_version()
    const {
  return kServerPublicKeyTestVersion;
}

void CertificateReportingServiceTestHelper::WaitForRequestsCreated(
    const ReportExpectation& expectation) {
  WaitReports(interceptor()->request_created_observer(), expectation);
}

void CertificateReportingServiceTestHelper::WaitForRequestsDestroyed(
    const ReportExpectation& expectation) {
  WaitReports(interceptor()->request_destroyed_observer(), expectation);
}

void CertificateReportingServiceTestHelper::ExpectNoRequests(
    CertificateReportingService* service) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Check that all requests have been destroyed.
  EXPECT_TRUE(interceptor()
                  ->request_destroyed_observer()
                  ->successful_reports()
                  .empty());
  EXPECT_TRUE(
      interceptor()->request_destroyed_observer()->failed_reports().empty());
  EXPECT_TRUE(
      interceptor()->request_destroyed_observer()->delayed_reports().empty());

  if (service->GetReporterForTesting()) {
    // Reporter can be null if reporting is disabled.
    EXPECT_EQ(
        0u,
        service->GetReporterForTesting()->inflight_report_count_for_testing());
  }
}

}  // namespace certificate_reporting_test_utils
