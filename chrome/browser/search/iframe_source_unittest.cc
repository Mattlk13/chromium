// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/iframe_source.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/search/instant_io_context.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "ipc/ipc_message.h"
#include "net/base/request_priority.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

const int kNonInstantRendererPID = 0;
const char kNonInstantOrigin[] = "http://evil";
const int kInstantRendererPID = 1;
const char kInstantOrigin[] = "chrome-search://instant";
const int kInvalidRendererPID = 42;

class TestIframeSource : public IframeSource {
 public:
  using IframeSource::GetMimeType;
  using IframeSource::ShouldServiceRequest;
  using IframeSource::SendResource;
  using IframeSource::SendJSWithOrigin;

  void set_origin(std::string origin) { origin_ = origin; }

 protected:
  std::string GetSource() const override { return "test"; }

  bool ServesPath(const std::string& path) const override {
    return path == "/valid.html" || path == "/valid.js";
  }

  void StartDataRequest(
      const std::string& path,
      const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
      const content::URLDataSource::GotDataCallback& callback) override {}

  // RenderFrameHost is hard to mock in concert with everything else, so stub
  // this method out for testing.
  bool GetOrigin(
      const content::ResourceRequestInfo::WebContentsGetter& wc_getter,
      std::string* origin) const override {
    if (origin_.empty())
      return false;
    *origin = origin_;
    return true;
  }

 private:
  std::string origin_;
};

class IframeSourceTest : public testing::Test {
 public:
  // net::URLRequest wants to be executed with a message loop that has TYPE_IO.
  // InstantIOContext needs to be created on the UI thread and have everything
  // else happen on the IO thread. This setup is a hacky way to satisfy all
  // those constraints.
  IframeSourceTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        resource_context_(&test_url_request_context_),
        instant_io_context_(NULL),
        response_(NULL) {
  }

  TestIframeSource* source() { return source_.get(); }

  std::string response_string() {
    if (response_.get()) {
      return std::string(response_->front_as<char>(), response_->size());
    }
    return "";
  }

  std::unique_ptr<net::URLRequest> MockRequest(const std::string& url,
                                               int render_process_id) {
    std::unique_ptr<net::URLRequest> request(
        resource_context_.GetRequestContext()->CreateRequest(
            GURL(url), net::DEFAULT_PRIORITY, NULL));
    content::ResourceRequestInfo::AllocateForTesting(
        request.get(),
        content::RESOURCE_TYPE_SUB_FRAME,
        &resource_context_,
        render_process_id,
        MSG_ROUTING_NONE,
        MSG_ROUTING_NONE,
        false,   // is_main_frame
        false,   // parent_is_main_frame
        true,    // allow_download
        false,   // is_async
        false);  // is_using_lofi
    return request;
  }

  void SendResource(int resource_id) {
    source()->SendResource(resource_id, callback_);
  }

  void SendJSWithOrigin(int resource_id) {
    source()->SendJSWithOrigin(
        resource_id, content::ResourceRequestInfo::WebContentsGetter(),
        callback_);
  }

 private:
  void SetUp() override {
    source_.reset(new TestIframeSource());
    callback_ = base::Bind(&IframeSourceTest::SaveResponse,
                           base::Unretained(this));
    instant_io_context_ = new InstantIOContext;
    InstantIOContext::SetUserDataOnIO(&resource_context_, instant_io_context_);
    source_->set_origin(kInstantOrigin);
    InstantIOContext::AddInstantProcessOnIO(instant_io_context_,
                                            kInstantRendererPID);
    response_ = NULL;
  }

  void TearDown() override { source_.reset(); }

  void SaveResponse(scoped_refptr<base::RefCountedMemory> data) {
    response_ = data;
  }

  content::TestBrowserThreadBundle thread_bundle_;

  net::TestURLRequestContext test_url_request_context_;
  content::MockResourceContext resource_context_;
  std::unique_ptr<TestIframeSource> source_;
  content::URLDataSource::GotDataCallback callback_;
  scoped_refptr<InstantIOContext> instant_io_context_;
  scoped_refptr<base::RefCountedMemory> response_;
};

TEST_F(IframeSourceTest, ShouldServiceRequest) {
  std::unique_ptr<net::URLRequest> request;
  source()->set_origin(kNonInstantOrigin);
  request = MockRequest("http://test/loader.js", kNonInstantRendererPID);
  EXPECT_FALSE(source()->ShouldServiceRequest(request.get()));
  source()->set_origin(kInstantOrigin);
  request = MockRequest("chrome-search://bogus/valid.js", kInstantRendererPID);
  EXPECT_FALSE(source()->ShouldServiceRequest(request.get()));
  source()->set_origin(kInstantOrigin);
  request = MockRequest("chrome-search://test/bogus.js", kInstantRendererPID);
  EXPECT_FALSE(source()->ShouldServiceRequest(request.get()));
  source()->set_origin(kInstantOrigin);
  request = MockRequest("chrome-search://test/valid.js", kInstantRendererPID);
  EXPECT_TRUE(source()->ShouldServiceRequest(request.get()));
  source()->set_origin(kNonInstantOrigin);
  request = MockRequest("chrome-search://test/valid.js",
                        kNonInstantRendererPID);
  EXPECT_FALSE(source()->ShouldServiceRequest(request.get()));
  source()->set_origin(std::string());
  request = MockRequest("chrome-search://test/valid.js", kInvalidRendererPID);
  EXPECT_FALSE(source()->ShouldServiceRequest(request.get()));
}

TEST_F(IframeSourceTest, GetMimeType) {
  // URLDataManagerBackend does not include / in path_and_query.
  EXPECT_EQ("text/html", source()->GetMimeType("foo.html"));
  EXPECT_EQ("application/javascript", source()->GetMimeType("foo.js"));
  EXPECT_EQ("text/css", source()->GetMimeType("foo.css"));
  EXPECT_EQ("image/png", source()->GetMimeType("foo.png"));
  EXPECT_EQ("", source()->GetMimeType("bogus"));
}

TEST_F(IframeSourceTest, SendResource) {
  SendResource(IDR_MOST_VISITED_TITLE_HTML);
  EXPECT_FALSE(response_string().empty());
}

TEST_F(IframeSourceTest, SendJSWithOrigin) {
  source()->set_origin(kInstantOrigin);
  SendJSWithOrigin(IDR_MOST_VISITED_TITLE_JS);
  EXPECT_FALSE(response_string().empty());
  source()->set_origin(kNonInstantOrigin);
  SendJSWithOrigin(IDR_MOST_VISITED_TITLE_JS);
  EXPECT_FALSE(response_string().empty());
  source()->set_origin(std::string());
  SendJSWithOrigin(IDR_MOST_VISITED_TITLE_JS);
  EXPECT_TRUE(response_string().empty());
}
