// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/toolbar/toolbar_model_impl_ios.h"

#include "components/bookmarks/browser/bookmark_model.h"
#include "components/toolbar/toolbar_model_impl.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/reading_list/offline_url_utils.h"
#import "ios/chrome/browser/tabs/tab.h"
#include "ios/chrome/browser/ui/toolbar/toolbar_model_delegate_ios.h"
#import "ios/web/public/navigation_item.h"
#import "ios/web/public/web_state/web_state.h"

namespace {
const size_t kMaxURLDisplayChars = 32 * 1024;

bookmarks::BookmarkModel* GetBookmarkModelForTab(Tab* tab) {
  web::WebState* web_state = [tab webState];
  if (!web_state)
    return nullptr;
  web::BrowserState* browser_state = web_state->GetBrowserState();
  if (!browser_state)
    return nullptr;
  return ios::BookmarkModelFactory::GetForBrowserState(
      ios::ChromeBrowserState::FromBrowserState(browser_state));
}
}  // namespace

ToolbarModelImplIOS::ToolbarModelImplIOS(ToolbarModelDelegateIOS* delegate) {
  delegate_ = delegate;
  toolbar_model_.reset(new ToolbarModelImpl(delegate, kMaxURLDisplayChars));
}

ToolbarModelImplIOS::~ToolbarModelImplIOS() {}

bool ToolbarModelImplIOS::IsLoading() {
  // Please note, ToolbarModel's notion of isLoading is slightly different from
  // WebState's IsLoading().
  web::WebState* web_state = delegate_->GetCurrentTab().webState;
  return web_state && web_state->IsLoading() && !IsCurrentTabNativePage();
}

CGFloat ToolbarModelImplIOS::GetLoadProgressFraction() {
  web::WebState* webState = delegate_->GetCurrentTab().webState;
  return webState ? webState->GetLoadingProgress() : 0.0;
}

bool ToolbarModelImplIOS::CanGoBack() {
  return delegate_->GetCurrentTab().canGoBack;
}

bool ToolbarModelImplIOS::CanGoForward() {
  return delegate_->GetCurrentTab().canGoForward;
}

bool ToolbarModelImplIOS::IsCurrentTabNativePage() {
  Tab* current_tab = delegate_->GetCurrentTab();
  return current_tab && current_tab.url.SchemeIs(kChromeUIScheme);
}

bool ToolbarModelImplIOS::IsCurrentTabBookmarked() {
  Tab* current_tab = delegate_->GetCurrentTab();
  bookmarks::BookmarkModel* bookmarkModel = GetBookmarkModelForTab(current_tab);
  return current_tab && bookmarkModel &&
         bookmarkModel->IsBookmarked(current_tab.url);
}

bool ToolbarModelImplIOS::IsCurrentTabBookmarkedByUser() {
  Tab* current_tab = delegate_->GetCurrentTab();
  bookmarks::BookmarkModel* bookmarkModel = GetBookmarkModelForTab(current_tab);
  return current_tab && bookmarkModel &&
         bookmarkModel->GetMostRecentlyAddedUserNodeForURL(current_tab.url);
}

bool ToolbarModelImplIOS::ShouldDisplayHintText() {
  Tab* current_tab = delegate_->GetCurrentTab();
  return [current_tab.webController wantsLocationBarHintText];
}

base::string16 ToolbarModelImplIOS::GetFormattedURL(size_t* prefix_end) const {
  base::string16 formatted_url = toolbar_model_->GetFormattedURL(prefix_end);
  Tab* current_tab = delegate_->GetCurrentTab();
  if (!current_tab || !current_tab.webState ||
      !current_tab.webState->GetNavigationManager() ||
      !current_tab.webState->GetNavigationManager()->GetVisibleItem()) {
    return formatted_url;
  }
  GURL url =
      current_tab.webState->GetNavigationManager()->GetVisibleItem()->GetURL();
  if (reading_list::IsOfflineURL(url) &&
      GetSecurityLevel(true /*ignore_editing*/) ==
          security_state::SecurityLevel::NONE) {
    size_t removed = 0;
    formatted_url =
        reading_list::StripSchemeFromOnlineURL(formatted_url, &removed);
    if (prefix_end) {
      *prefix_end -= removed;
    }
  }
  return formatted_url;
}

GURL ToolbarModelImplIOS::GetURL() const {
  return toolbar_model_->GetURL();
}

security_state::SecurityLevel ToolbarModelImplIOS::GetSecurityLevel(
    bool ignore_editing) const {
  return toolbar_model_->GetSecurityLevel(ignore_editing);
}

gfx::VectorIconId ToolbarModelImplIOS::GetVectorIcon() const {
  return toolbar_model_->GetVectorIcon();
}

base::string16 ToolbarModelImplIOS::GetSecureVerboseText() const {
  return toolbar_model_->GetSecureVerboseText();
}

base::string16 ToolbarModelImplIOS::GetEVCertName() const {
  return toolbar_model_->GetEVCertName();
}

bool ToolbarModelImplIOS::ShouldDisplayURL() const {
  return toolbar_model_->ShouldDisplayURL();
}
