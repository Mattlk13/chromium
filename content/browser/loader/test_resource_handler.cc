// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/test_resource_handler.h"

#include "base/logging.h"
#include "content/browser/loader/resource_controller.h"
#include "content/public/common/resource_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class ScopedCallDepthTracker {
 public:
  explicit ScopedCallDepthTracker(int* call_depth) : call_depth_(call_depth) {
    EXPECT_EQ(0, *call_depth_);
    (*call_depth_)++;
  }

  ~ScopedCallDepthTracker() {
    EXPECT_EQ(1, *call_depth_);
    (*call_depth_)--;
  }

 private:
  int* const call_depth_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCallDepthTracker);
};

}  // namespace

TestResourceHandler::TestResourceHandler(net::URLRequestStatus* request_status,
                                         std::string* body)
    : ResourceHandler(nullptr),
      request_status_ptr_(request_status),
      body_ptr_(body),
      deferred_run_loop_(new base::RunLoop()) {
  SetBufferSize(2048);
}

TestResourceHandler::TestResourceHandler()
    : TestResourceHandler(nullptr, nullptr) {}

TestResourceHandler::~TestResourceHandler() {}

void TestResourceHandler::SetController(ResourceController* controller) {
  controller_ = controller;
}

bool TestResourceHandler::OnRequestRedirected(
    const net::RedirectInfo& redirect_info,
    ResourceResponse* response,
    bool* defer) {
  EXPECT_FALSE(canceled_);
  EXPECT_EQ(1, on_will_start_called_);
  EXPECT_EQ(0, on_response_started_called_);
  EXPECT_EQ(0, on_response_completed_called_);
  ScopedCallDepthTracker call_depth_tracker(&call_depth_);

  ++on_request_redirected_called_;

  if (!on_request_redirected_result_) {
    canceled_ = true;
    return false;
  }

  *defer = defer_on_request_redirected_;
  defer_on_request_redirected_ = false;
  if (*defer)
    deferred_run_loop_->Quit();
  return true;
}

bool TestResourceHandler::OnResponseStarted(ResourceResponse* response,
                                            bool* defer) {
  EXPECT_FALSE(canceled_);
  EXPECT_EQ(1, on_will_start_called_);
  EXPECT_EQ(0, on_response_started_called_);
  EXPECT_EQ(0, on_response_completed_called_);
  ScopedCallDepthTracker call_depth_tracker(&call_depth_);

  ++on_response_started_called_;

  EXPECT_FALSE(resource_response_);
  resource_response_ = response;

  if (!on_response_started_result_) {
    canceled_ = true;
    return false;
  }

  *defer = defer_on_response_started_;
  defer_on_response_started_ = false;
  if (*defer)
    deferred_run_loop_->Quit();
  return true;
}

bool TestResourceHandler::OnWillStart(const GURL& url, bool* defer) {
  EXPECT_FALSE(canceled_);
  EXPECT_EQ(0, on_response_started_called_);
  EXPECT_EQ(0, on_will_start_called_);
  EXPECT_EQ(0, on_response_completed_called_);
  ScopedCallDepthTracker call_depth_tracker(&call_depth_);

  ++on_will_start_called_;

  start_url_ = url;

  if (!on_will_start_result_) {
    canceled_ = true;
    return false;
  }

  *defer = defer_on_will_start_;
  if (*defer)
    deferred_run_loop_->Quit();
  return true;
}

bool TestResourceHandler::OnWillRead(scoped_refptr<net::IOBuffer>* buf,
                                     int* buf_size,
                                     int min_size) {
  EXPECT_FALSE(canceled_);
  EXPECT_FALSE(expect_on_data_downloaded_);
  EXPECT_EQ(0, on_response_completed_called_);
  ScopedCallDepthTracker call_depth_tracker(&call_depth_);

  ++on_will_read_called_;

  *buf = buffer_;
  *buf_size = buffer_size_;
  memset(buffer_->data(), '\0', buffer_size_);
  if (!on_will_read_result_)
    canceled_ = true;
  return on_will_read_result_;
}

bool TestResourceHandler::OnReadCompleted(int bytes_read, bool* defer) {
  EXPECT_FALSE(canceled_);
  EXPECT_FALSE(expect_on_data_downloaded_);
  EXPECT_EQ(1, on_will_start_called_);
  EXPECT_EQ(1, on_response_started_called_);
  EXPECT_EQ(0, on_response_completed_called_);
  EXPECT_EQ(0, on_read_eof_);
  ScopedCallDepthTracker call_depth_tracker(&call_depth_);

  ++on_read_completed_called_;
  if (bytes_read == 0)
    ++on_read_eof_;

  EXPECT_LE(static_cast<size_t>(bytes_read), buffer_size_);
  if (body_ptr_)
    body_ptr_->append(buffer_->data(), bytes_read);
  body_.append(buffer_->data(), bytes_read);

  if (!on_read_completed_result_ || (!on_read_eof_result_ && bytes_read == 0)) {
    canceled_ = true;
    return false;
  }

  *defer = defer_on_read_completed_;
  defer_on_read_completed_ = false;
  if (bytes_read == 0 && defer_on_read_eof_)
    *defer = true;

  if (*defer)
    deferred_run_loop_->Quit();

  return true;
}

void TestResourceHandler::OnResponseCompleted(
    const net::URLRequestStatus& status,
    bool* defer) {
  ScopedCallDepthTracker call_depth_tracker(&call_depth_);

  EXPECT_EQ(0, on_response_completed_called_);
  if (status.is_success() && !expect_on_data_downloaded_ && expect_eof_read_)
    EXPECT_EQ(1, on_read_eof_);

  ++on_response_completed_called_;

  if (request_status_ptr_)
    *request_status_ptr_ = status;
  final_status_ = status;
  *defer = defer_on_response_completed_;
  defer_on_response_completed_ = false;

  if (*defer)
    deferred_run_loop_->Quit();
  response_complete_run_loop_.Quit();
}

void TestResourceHandler::OnDataDownloaded(int bytes_downloaded) {
  EXPECT_TRUE(expect_on_data_downloaded_);
  total_bytes_downloaded_ += bytes_downloaded;
}

void TestResourceHandler::Resume() {
  ScopedCallDepthTracker call_depth_tracker(&call_depth_);
  controller_->Resume();
}

void TestResourceHandler::CancelWithError(net::Error net_error) {
  ScopedCallDepthTracker call_depth_tracker(&call_depth_);
  canceled_ = true;
  controller_->CancelWithError(net_error);
}

void TestResourceHandler::SetBufferSize(int buffer_size) {
  buffer_ = new net::IOBuffer(buffer_size);
  buffer_size_ = buffer_size;
  memset(buffer_->data(), '\0', buffer_size);
}

void TestResourceHandler::WaitUntilDeferred() {
  deferred_run_loop_->Run();
  deferred_run_loop_.reset(new base::RunLoop());
}

void TestResourceHandler::WaitUntilResponseComplete() {
  response_complete_run_loop_.Run();
}

}  // namespace content
