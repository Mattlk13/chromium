/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebDataSource_h
#define WebDataSource_h

#include "../platform/WebCommon.h"
#include "WebNavigationType.h"
#include "WebTextDirection.h"

namespace blink {

class WebDocumentSubresourceFilter;
class WebURL;
class WebURLRequest;
class WebURLResponse;
template <typename T>
class WebVector;

class WebDataSource {
 public:
  class ExtraData {
   public:
    virtual ~ExtraData() {}
  };

  // Returns the original request that resulted in this datasource.
  virtual const WebURLRequest& originalRequest() const = 0;

  // Returns the request corresponding to this datasource.  It may
  // include additional request headers added by WebKit that were not
  // present in the original request.  This request may also correspond
  // to a location specified by a redirect that was followed.
  virtual const WebURLRequest& getRequest() const = 0;

  // Returns the response associated with this datasource.
  virtual const WebURLResponse& response() const = 0;

  // When this datasource was created as a result of WebFrame::loadData,
  // there may be an associated unreachableURL.
  virtual bool hasUnreachableURL() const = 0;
  virtual WebURL unreachableURL() const = 0;

  // Allows the embedder to append redirects to the chain as a navigation
  // is starting, in case it is being transferred from another process.
  virtual void appendRedirect(const WebURL&) = 0;

  // Returns all redirects that occurred (both client and server) before
  // at last committing the current page.  This will contain one entry
  // for each intermediate URL, and one entry for the last URL (so if
  // there are no redirects, it will contain exactly the current URL, and
  // if there is one redirect, it will contain the source and destination
  // URL).
  virtual void redirectChain(WebVector<WebURL>&) const = 0;

  // Returns whether the navigation associated with this datasource is a
  // client redirect.
  virtual bool isClientRedirect() const = 0;

  // Returns whether the navigation associated with this datasource should
  // replace the current history item.
  virtual bool replacesCurrentHistoryItem() const = 0;

  // The type of navigation that triggered the creation of this datasource.
  virtual WebNavigationType navigationType() const = 0;

  // Extra data associated with this datasource.  If non-null, the extra
  // data pointer will be deleted when the datasource is destroyed.
  // Setting the extra data pointer will cause any existing non-null
  // extra data pointer to be deleted.
  virtual ExtraData* getExtraData() const = 0;
  virtual void setExtraData(ExtraData*) = 0;

  // Sets the navigation start time for this datasource. Ordinarily,
  // navigation start is determined in WebCore. But, in some situations,
  // the embedder might have a better value and can override it here. This
  // should be called before WebFrameClient::didCommitProvisionalLoad.
  // Calling it later may confuse users, because JavaScript may have run and
  // the user may have already recorded the original value.
  virtual void setNavigationStartTime(double) = 0;

  // Sets timing and attributes of the navigation.
  // Ordinarily, they are determined in WebCore, but when the navigation is
  // handled by the client, they can be passed here.
  virtual void updateNavigation(double redirectStartTime,
                                double redirectEndTime,
                                double fetchStartTime,
                                const WebVector<WebURL>& redirectChain) = 0;

  // Allows the embedder to inject a filter that will be consulted for each
  // subsequent subresource load, and gets the final say in deciding whether
  // or not to allow the load. The passed-in filter object is deleted when the
  // datasource is destroyed or when a new filter is set.
  virtual void setSubresourceFilter(WebDocumentSubresourceFilter*) = 0;

 protected:
  ~WebDataSource() {}
};

}  // namespace blink

#endif
