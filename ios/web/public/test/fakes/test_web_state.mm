// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/fakes/test_web_state.h"

#include <stdint.h>

#include "base/callback.h"
#include "ios/web/public/web_state/web_state_observer.h"

namespace web {

void TestWebState::AddObserver(WebStateObserver* observer) {
  observers_.AddObserver(observer);
}

void TestWebState::RemoveObserver(WebStateObserver* observer) {
  observers_.RemoveObserver(observer);
}

TestWebState::TestWebState()
    : web_usage_enabled_(false),
      is_loading_(false),
      trust_level_(kAbsolute),
      content_is_html_(true) {}

TestWebState::~TestWebState() {
  for (auto& observer : observers_)
    observer.WebStateDestroyed();
  for (auto& observer : observers_)
    observer.ResetWebState();
};

WebStateDelegate* TestWebState::GetDelegate() {
  return nil;
}

void TestWebState::SetDelegate(WebStateDelegate* delegate) {}

BrowserState* TestWebState::GetBrowserState() const {
  return nullptr;
}

bool TestWebState::IsWebUsageEnabled() const {
  return web_usage_enabled_;
}

void TestWebState::SetWebUsageEnabled(bool enabled) {
  web_usage_enabled_ = enabled;
}

bool TestWebState::ShouldSuppressDialogs() const {
  return false;
}

void TestWebState::SetShouldSuppressDialogs(bool should_suppress) {}

UIView* TestWebState::GetView() {
  return view_.get();
}

const NavigationManager* TestWebState::GetNavigationManager() const {
  return navigation_manager_.get();
}

NavigationManager* TestWebState::GetNavigationManager() {
  return navigation_manager_.get();
}

void TestWebState::SetNavigationManager(
    std::unique_ptr<NavigationManager> navigation_manager) {
  navigation_manager_ = std::move(navigation_manager);
}

void TestWebState::SetView(UIView* view) {
  view_.reset([view retain]);
}

CRWJSInjectionReceiver* TestWebState::GetJSInjectionReceiver() const {
  return nullptr;
}

void TestWebState::ExecuteJavaScript(const base::string16& javascript) {}

void TestWebState::ExecuteJavaScript(const base::string16& javascript,
                                     const JavaScriptResultCallback& callback) {
  callback.Run(nullptr);
}

const std::string& TestWebState::GetContentsMimeType() const {
  return mime_type_;
}

const std::string& TestWebState::GetContentLanguageHeader() const {
  return content_language_;
}

bool TestWebState::ContentIsHTML() const {
  return content_is_html_;
}

const GURL& TestWebState::GetVisibleURL() const {
  return url_;
}

const GURL& TestWebState::GetLastCommittedURL() const {
  return url_;
}

GURL TestWebState::GetCurrentURL(URLVerificationTrustLevel* trust_level) const {
  *trust_level = trust_level_;
  return url_;
}

bool TestWebState::IsShowingWebInterstitial() const {
  return false;
}

WebInterstitial* TestWebState::GetWebInterstitial() const {
  return nullptr;
}

void TestWebState::SetContentIsHTML(bool content_is_html) {
  content_is_html_ = content_is_html;
}

const base::string16& TestWebState::GetTitle() const {
  return title_;
}

bool TestWebState::IsLoading() const {
  return is_loading_;
}

double TestWebState::GetLoadingProgress() const {
  return 0.0;
}

bool TestWebState::IsBeingDestroyed() const {
  return false;
}

void TestWebState::SetLoading(bool is_loading) {
  if (is_loading == is_loading_)
    return;

  is_loading_ = is_loading;

  if (is_loading) {
    for (auto& observer : observers_)
      observer.DidStartLoading();
  } else {
    for (auto& observer : observers_)
      observer.DidStopLoading();
  }
}

void TestWebState::OnPageLoaded(
    PageLoadCompletionStatus load_completion_status) {
  for (auto& observer : observers_)
    observer.PageLoaded(load_completion_status);
}

void TestWebState::SetCurrentURL(const GURL& url) {
  url_ = url;
}

void TestWebState::SetTrustLevel(URLVerificationTrustLevel trust_level) {
  trust_level_ = trust_level;
}

CRWWebViewProxyType TestWebState::GetWebViewProxy() const {
  return nullptr;
}

int TestWebState::DownloadImage(const GURL& url,
                                bool is_favicon,
                                uint32_t max_bitmap_size,
                                bool bypass_cache,
                                const ImageDownloadCallback& callback) {
  return 0;
}

service_manager::InterfaceRegistry* TestWebState::GetMojoInterfaceRegistry() {
  return nullptr;
}

base::WeakPtr<WebState> TestWebState::AsWeakPtr() {
  NOTREACHED();
  return base::WeakPtr<WebState>();
}

}  // namespace web
