// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_context_request_handler.h"

#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/fileapi/mock_url_request_delegate.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_test_utils.h"
#include "content/browser/service_worker/service_worker_write_to_cache_job.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_request_context.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

void EmptyCallback() {}

}

class ServiceWorkerContextRequestHandlerTest : public testing::Test {
 public:
  ServiceWorkerContextRequestHandlerTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));

    // A new unstored registration/version.
    scope_ = GURL("http://host/scope/");
    script_url_ = GURL("http://host/script.js");
    registration_ = new ServiceWorkerRegistration(
        scope_, 1L, context()->AsWeakPtr());
    version_ = new ServiceWorkerVersion(
        registration_.get(), script_url_, 1L, context()->AsWeakPtr());

    // An empty host.
    std::unique_ptr<ServiceWorkerProviderHost> host(
        new ServiceWorkerProviderHost(
            helper_->mock_render_process_id(),
            MSG_ROUTING_NONE /* render_frame_id */, 1 /* provider_id */,
            SERVICE_WORKER_PROVIDER_FOR_CONTROLLER,
            ServiceWorkerProviderHost::FrameSecurityLevel::SECURE,
            context()->AsWeakPtr(), nullptr));
    provider_host_ = host->AsWeakPtr();
    context()->AddProviderHost(std::move(host));

    context()->storage()->LazyInitialize(base::Bind(&EmptyCallback));
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    version_ = nullptr;
    registration_ = nullptr;
    helper_.reset();
  }

  ServiceWorkerContextCore* context() const { return helper_->context(); }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> version_;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host_;
  net::URLRequestContext url_request_context_;
  MockURLRequestDelegate url_request_delegate_;
  GURL scope_;
  GURL script_url_;
  storage::BlobStorageContext blob_storage_context_;
};

class ServiceWorkerContextRequestHandlerTestP
    : public MojoServiceWorkerTestP<ServiceWorkerContextRequestHandlerTest> {};

TEST_P(ServiceWorkerContextRequestHandlerTestP, UpdateBefore24Hours) {
  // Give the registration a very recent last update time and pretend
  // we're installing a new version.
  registration_->set_last_update_check(base::Time::Now());
  version_->SetStatus(ServiceWorkerVersion::NEW);
  provider_host_->running_hosted_version_ = version_;

  // Conduct a resource fetch for the main script.
  const GURL kScriptUrl("http://host/script.js");
  std::unique_ptr<net::URLRequest> request = url_request_context_.CreateRequest(
      kScriptUrl, net::DEFAULT_PRIORITY, &url_request_delegate_);
  std::unique_ptr<ServiceWorkerContextRequestHandler> handler(
      new ServiceWorkerContextRequestHandler(
          context()->AsWeakPtr(), provider_host_,
          base::WeakPtr<storage::BlobStorageContext>(),
          RESOURCE_TYPE_SERVICE_WORKER));
  std::unique_ptr<net::URLRequestJob> job(
      handler->MaybeCreateJob(request.get(), nullptr, nullptr));
  ASSERT_TRUE(job.get());
  ServiceWorkerWriteToCacheJob* sw_job =
      static_cast<ServiceWorkerWriteToCacheJob*>(job.get());

  // Verify the net request is not initialized to bypass the browser cache.
  EXPECT_FALSE(sw_job->net_request_->load_flags() & net::LOAD_BYPASS_CACHE);
}

TEST_P(ServiceWorkerContextRequestHandlerTestP, UpdateAfter24Hours) {
  // Give the registration a old update time and pretend
  // we're installing a new version.
  registration_->set_last_update_check(
      base::Time::Now() - base::TimeDelta::FromDays(7));
  version_->SetStatus(ServiceWorkerVersion::NEW);
  provider_host_->running_hosted_version_ = version_;

  // Conduct a resource fetch for the main script.
  const GURL kScriptUrl("http://host/script.js");
  std::unique_ptr<net::URLRequest> request = url_request_context_.CreateRequest(
      kScriptUrl, net::DEFAULT_PRIORITY, &url_request_delegate_);
  std::unique_ptr<ServiceWorkerContextRequestHandler> handler(
      new ServiceWorkerContextRequestHandler(
          context()->AsWeakPtr(), provider_host_,
          base::WeakPtr<storage::BlobStorageContext>(),
          RESOURCE_TYPE_SERVICE_WORKER));
  std::unique_ptr<net::URLRequestJob> job(
      handler->MaybeCreateJob(request.get(), nullptr, nullptr));
  ASSERT_TRUE(job.get());
  ServiceWorkerWriteToCacheJob* sw_job =
      static_cast<ServiceWorkerWriteToCacheJob*>(job.get());

  // Verify the net request is initialized to bypass the browser cache.
  EXPECT_TRUE(sw_job->net_request_->load_flags() & net::LOAD_BYPASS_CACHE);
}

TEST_P(ServiceWorkerContextRequestHandlerTestP, UpdateForceBypassCache) {
  // Give the registration a very recent last update time and pretend
  // we're installing a new version.
  registration_->set_last_update_check(base::Time::Now());
  version_->SetStatus(ServiceWorkerVersion::NEW);
  version_->set_force_bypass_cache_for_scripts(true);
  provider_host_->running_hosted_version_ = version_;

  // Conduct a resource fetch for the main script.
  const GURL kScriptUrl("http://host/script.js");
  std::unique_ptr<net::URLRequest> request = url_request_context_.CreateRequest(
      kScriptUrl, net::DEFAULT_PRIORITY, &url_request_delegate_);
  std::unique_ptr<ServiceWorkerContextRequestHandler> handler(
      new ServiceWorkerContextRequestHandler(
          context()->AsWeakPtr(), provider_host_,
          base::WeakPtr<storage::BlobStorageContext>(),
          RESOURCE_TYPE_SERVICE_WORKER));
  std::unique_ptr<net::URLRequestJob> job(
      handler->MaybeCreateJob(request.get(), nullptr, nullptr));
  ASSERT_TRUE(job.get());
  ServiceWorkerWriteToCacheJob* sw_job =
      static_cast<ServiceWorkerWriteToCacheJob*>(job.get());

  // Verify the net request is initialized to bypass the browser cache.
  EXPECT_TRUE(sw_job->net_request_->load_flags() & net::LOAD_BYPASS_CACHE);
}

TEST_P(ServiceWorkerContextRequestHandlerTestP,
       ServiceWorkerDataRequestAnnotation) {
  version_->SetStatus(ServiceWorkerVersion::NEW);
  provider_host_->running_hosted_version_ = version_;

  // Conduct a resource fetch for the main script.
  const GURL kScriptUrl("http://host/script.js");
  std::unique_ptr<net::URLRequest> request = url_request_context_.CreateRequest(
      kScriptUrl, net::DEFAULT_PRIORITY, &url_request_delegate_);
  std::unique_ptr<ServiceWorkerContextRequestHandler> handler(
      new ServiceWorkerContextRequestHandler(
          context()->AsWeakPtr(), provider_host_,
          base::WeakPtr<storage::BlobStorageContext>(),
          RESOURCE_TYPE_SERVICE_WORKER));
  std::unique_ptr<net::URLRequestJob> job(
      handler->MaybeCreateJob(request.get(), nullptr, nullptr));
  ASSERT_TRUE(job.get());
  ServiceWorkerWriteToCacheJob* sw_job =
      static_cast<ServiceWorkerWriteToCacheJob*>(job.get());

  // Verify that the request is properly annotated as originating from a
  // Service Worker.
  EXPECT_TRUE(ResourceRequestInfo::OriginatedFromServiceWorker(
      sw_job->net_request_.get()));
}

// Tests starting a service worker when the skip_service_worker flag is on. The
// flag should be ignored.
TEST_P(ServiceWorkerContextRequestHandlerTestP,
       SkipServiceWorkerForServiceWorkerRequest) {
  // Conduct a resource fetch for the main script.
  version_->SetStatus(ServiceWorkerVersion::NEW);
  provider_host_->running_hosted_version_ = version_;
  const GURL kScriptUrl("http://host/script.js");
  std::unique_ptr<net::URLRequest> request = url_request_context_.CreateRequest(
      kScriptUrl, net::DEFAULT_PRIORITY, &url_request_delegate_);
  ServiceWorkerRequestHandler::InitializeHandler(
      request.get(), helper_->context_wrapper(), &blob_storage_context_,
      helper_->mock_render_process_id(), provider_host_->provider_id(),
      true /* skip_service_worker */, FETCH_REQUEST_MODE_NO_CORS,
      FETCH_CREDENTIALS_MODE_OMIT, FetchRedirectMode::FOLLOW_MODE,
      RESOURCE_TYPE_SERVICE_WORKER, REQUEST_CONTEXT_TYPE_SERVICE_WORKER,
      REQUEST_CONTEXT_FRAME_TYPE_NONE, nullptr);
  // Verify a ServiceWorkerRequestHandler was created.
  ServiceWorkerRequestHandler* handler =
      ServiceWorkerRequestHandler::GetHandler(request.get());
  EXPECT_TRUE(handler);
}

INSTANTIATE_TEST_CASE_P(ServiceWorkerContextRequestHandlerTest,
                        ServiceWorkerContextRequestHandlerTestP,
                        testing::Bool());

}  // namespace content
