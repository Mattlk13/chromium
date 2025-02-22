// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_THROTTLING_RESOURCE_HANDLER_H_
#define CONTENT_BROWSER_LOADER_THROTTLING_RESOURCE_HANDLER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "content/browser/loader/layered_resource_handler.h"
#include "content/public/browser/resource_throttle.h"
#include "net/url_request/redirect_info.h"
#include "url/gurl.h"

namespace net {
class URLRequest;
}

namespace content {

struct ResourceResponse;

// Used to apply a list of ResourceThrottle instances to an URLRequest.
class ThrottlingResourceHandler : public LayeredResourceHandler,
                                  public ResourceThrottle::Delegate {
 public:
  ThrottlingResourceHandler(
      std::unique_ptr<ResourceHandler> next_handler,
      net::URLRequest* request,
      std::vector<std::unique_ptr<ResourceThrottle>> throttles);
  ~ThrottlingResourceHandler() override;

  // LayeredResourceHandler overrides:
  bool OnRequestRedirected(const net::RedirectInfo& redirect_info,
                           ResourceResponse* response,
                           bool* defer) override;
  bool OnResponseStarted(ResourceResponse* response, bool* defer) override;
  bool OnWillStart(const GURL& url, bool* defer) override;

  // ResourceThrottle::Delegate implementation:
  void Cancel() override;
  void CancelAndIgnore() override;
  void CancelWithError(int error_code) override;
  void Resume() override;

 private:
  void ResumeStart();
  void ResumeRedirect();
  void ResumeResponse();

  // Called when the throttle at |throttle_index| defers a request.  Logs the
  // name of the throttle that delayed the request.
  void OnRequestDeferred(int throttle_index);

  enum DeferredStage {
    DEFERRED_NONE,
    DEFERRED_START,
    DEFERRED_REDIRECT,
    DEFERRED_RESPONSE
  };
  DeferredStage deferred_stage_;

  std::vector<std::unique_ptr<ResourceThrottle>> throttles_;
  size_t next_index_;

  GURL deferred_url_;
  net::RedirectInfo deferred_redirect_;
  scoped_refptr<ResourceResponse> deferred_response_;

  bool cancelled_by_resource_throttle_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_THROTTLING_RESOURCE_HANDLER_H_
