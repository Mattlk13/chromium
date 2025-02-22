// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installable_manager.h"

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "chrome/browser/manifest/manifest_icon_downloader.h"
#include "chrome/browser/manifest/manifest_icon_selector.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "third_party/WebKit/public/platform/WebDisplayMode.h"

namespace {

const char kPngExtension[] = ".png";

// This constant is the icon size on Android (48dp) multiplied by the scale
// factor of a Nexus 5 device (3x). For mobile and desktop platforms, a 144px
// icon is an approximate, appropriate lower bound. It is the currently
// advertised minimum icon size for triggering banners.
// TODO(dominickn): consolidate with minimum_icon_size_in_px across platforms.
const int kIconMinimumSizeInPx = 144;

// Returns true if |manifest| specifies a PNG icon >= 144x144px (or size "any").
bool DoesManifestContainRequiredIcon(const content::Manifest& manifest) {
  for (const auto& icon : manifest.icons) {
    // The type field is optional. If it isn't present, fall back on checking
    // the src extension, and allow the icon if the extension ends with png.
    if (!base::EqualsASCII(icon.type, "image/png") &&
        !(icon.type.empty() && base::EndsWith(
            icon.src.ExtractFileName(), kPngExtension,
            base::CompareCase::INSENSITIVE_ASCII)))
      continue;

    for (const auto& size : icon.sizes) {
      if (size.IsEmpty())  // "any"
        return true;
      if (size.width() >= kIconMinimumSizeInPx &&
          size.height() >= kIconMinimumSizeInPx) {
        return true;
      }
    }
  }

  return false;
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(InstallableManager);

struct InstallableManager::ManifestProperty {
  InstallableStatusCode error = NO_ERROR_DETECTED;
  GURL url;
  content::Manifest manifest;
  bool fetched = false;
};

struct InstallableManager::InstallableProperty {
  InstallableStatusCode error = NO_ERROR_DETECTED;
  bool installable = false;
  bool fetched = false;
};

struct InstallableManager::IconProperty {
  IconProperty() :
    error(NO_ERROR_DETECTED), url(), icon(), fetched(false) { }
  IconProperty(IconProperty&& other) = default;
  IconProperty& operator=(IconProperty&& other) = default;

  InstallableStatusCode error = NO_ERROR_DETECTED;
  GURL url;
  std::unique_ptr<SkBitmap> icon;
  bool fetched;

 private:
  // This class contains a std::unique_ptr and therefore must be move-only.
  DISALLOW_COPY_AND_ASSIGN(IconProperty);
};


InstallableManager::InstallableManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      manifest_(new ManifestProperty()),
      installable_(new InstallableProperty()),
      is_active_(false),
      weak_factory_(this) { }

InstallableManager::~InstallableManager() = default;

// static
int InstallableManager::GetMinimumIconSizeInPx() {
  return kIconMinimumSizeInPx;
}

void InstallableManager::GetData(const InstallableParams& params,
                                 const InstallableCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Return immediately if we're already working on a task. The new task will be
  // looked at once the current task is finished.
  tasks_.push_back({params, callback});
  if (is_active_)
    return;

  is_active_ = true;
  StartNextTask();
}

InstallableManager::IconProperty& InstallableManager::GetIcon(
    const InstallableParams& params) {
  return icons_[{params.ideal_icon_size_in_px, params.minimum_icon_size_in_px}];
}

bool InstallableManager::IsIconFetched(const InstallableParams& params) const {
  const auto it = icons_.find(
      {params.ideal_icon_size_in_px, params.minimum_icon_size_in_px});
  return it != icons_.end() && it->second.fetched;
}

void InstallableManager::SetIconFetched(const InstallableParams& params) {
  GetIcon(params).fetched = true;
}

InstallableStatusCode InstallableManager::GetErrorCode(
    const InstallableParams& params) {
  if (manifest_->error != NO_ERROR_DETECTED)
    return manifest_->error;

  if (params.check_installable && installable_->error != NO_ERROR_DETECTED)
    return installable_->error;

  if (params.fetch_valid_icon) {
    IconProperty& icon = GetIcon(params);
    if (icon.error != NO_ERROR_DETECTED)
      return icon.error;
  }

  return NO_ERROR_DETECTED;
}

InstallableStatusCode InstallableManager::manifest_error() const {
  return manifest_->error;
}

InstallableStatusCode InstallableManager::installable_error() const {
  return installable_->error;
}

void InstallableManager::set_installable_error(
    InstallableStatusCode error_code) {
  installable_->error = error_code;
}

InstallableStatusCode InstallableManager::icon_error(
    const InstallableManager::IconParams& icon_params) {
  return icons_[icon_params].error;
}

GURL& InstallableManager::icon_url(
    const InstallableManager::IconParams& icon_params) {
  return icons_[icon_params].url;
}

const SkBitmap* InstallableManager::icon(
    const InstallableManager::IconParams& icon_params) {
  return icons_[icon_params].icon.get();
}

content::WebContents* InstallableManager::GetWebContents() {
  content::WebContents* contents = web_contents();
  if (!contents || contents->IsBeingDestroyed())
    return nullptr;
  return contents;
}

bool InstallableManager::IsComplete(const InstallableParams& params) const {
  // Returns true if for all resources:
  //  a. the params did not request it, OR
  //  b. the resource has been fetched/checked.
  return manifest_->fetched &&
         (!params.check_installable || installable_->fetched) &&
         (!params.fetch_valid_icon || IsIconFetched(params));
}

void InstallableManager::Reset() {
  // Prevent any outstanding callbacks to or from this object from being called.
  weak_factory_.InvalidateWeakPtrs();
  tasks_.clear();
  icons_.clear();

  manifest_.reset(new ManifestProperty());
  installable_.reset(new InstallableProperty());

  is_active_ = false;
}

void InstallableManager::SetManifestDependentTasksComplete() {
  DCHECK(!tasks_.empty());
  const InstallableParams& params = tasks_[0].first;

  installable_->fetched = true;
  SetIconFetched(params);
}

void InstallableManager::RunCallback(const Task& task,
                                     InstallableStatusCode code) {
  const InstallableParams& params = task.first;
  IconProperty& icon = GetIcon(params);
  InstallableData data = {
      code,
      manifest_url(),
      manifest(),
      params.fetch_valid_icon ? icon.url : GURL::EmptyGURL(),
      params.fetch_valid_icon ? icon.icon.get() : nullptr,
      params.check_installable ? is_installable() : false};

  task.second.Run(data);
}

void InstallableManager::StartNextTask() {
  // If there's nothing to do, exit. Resources remain cached so any future calls
  // won't re-fetch anything that has already been retrieved.
  if (tasks_.empty()) {
    is_active_ = false;
    return;
  }

  DCHECK(is_active_);
  WorkOnTask();
}

void InstallableManager::WorkOnTask() {
  DCHECK(!tasks_.empty());
  const Task& task = tasks_[0];
  const InstallableParams& params = task.first;

  InstallableStatusCode code = GetErrorCode(params);
  if (code != NO_ERROR_DETECTED || IsComplete(params)) {
    RunCallback(task, code);
    tasks_.erase(tasks_.begin());
    StartNextTask();
    return;
  }

  if (!manifest_->fetched)
    FetchManifest();
  else if (params.check_installable && !installable_->fetched)
    CheckInstallable();
  else if (params.fetch_valid_icon && !IsIconFetched(params))
    CheckAndFetchBestIcon();
  else
    NOTREACHED();
}

void InstallableManager::FetchManifest() {
  DCHECK(!manifest_->fetched);

  content::WebContents* web_contents = GetWebContents();
  DCHECK(web_contents);

  web_contents->GetManifest(base::Bind(&InstallableManager::OnDidGetManifest,
                                       weak_factory_.GetWeakPtr()));
}

void InstallableManager::OnDidGetManifest(const GURL& manifest_url,
                                          const content::Manifest& manifest) {
  if (!GetWebContents())
    return;

  if (manifest_url.is_empty()) {
    manifest_->error = NO_MANIFEST;
    SetManifestDependentTasksComplete();
  } else if (manifest.IsEmpty()) {
    manifest_->error = MANIFEST_EMPTY;
    SetManifestDependentTasksComplete();
  }

  manifest_->url = manifest_url;
  manifest_->manifest = manifest;
  manifest_->fetched = true;
  WorkOnTask();
}

void InstallableManager::CheckInstallable() {
  DCHECK(!installable_->fetched);
  DCHECK(!manifest().IsEmpty());

  if (IsManifestValidForWebApp(manifest())) {
    CheckServiceWorker();
  } else {
    installable_->installable = false;
    installable_->fetched = true;
    WorkOnTask();
  }
}

bool InstallableManager::IsManifestValidForWebApp(
    const content::Manifest& manifest) {
  if (manifest.IsEmpty()) {
    installable_->error = MANIFEST_EMPTY;
    return false;
  }

  if (!manifest.start_url.is_valid()) {
    installable_->error = START_URL_NOT_VALID;
    return false;
  }

  if ((manifest.name.is_null() || manifest.name.string().empty()) &&
      (manifest.short_name.is_null() || manifest.short_name.string().empty())) {
    installable_->error = MANIFEST_MISSING_NAME_OR_SHORT_NAME;
    return false;
  }

  // TODO(dominickn,mlamouri): when Chrome supports "minimal-ui", it should be
  // accepted. If we accept it today, it would fallback to "browser" and make
  // this check moot. See https://crbug.com/604390.
  if (manifest.display != blink::WebDisplayModeStandalone &&
      manifest.display != blink::WebDisplayModeFullscreen) {
    installable_->error = MANIFEST_DISPLAY_NOT_SUPPORTED;
    return false;
  }

  if (!DoesManifestContainRequiredIcon(manifest)) {
    installable_->error = MANIFEST_MISSING_SUITABLE_ICON;
    return false;
  }

  return true;
}

void InstallableManager::CheckServiceWorker() {
  DCHECK(!installable_->fetched);
  DCHECK(!manifest().IsEmpty());
  DCHECK(manifest().start_url.is_valid());

  content::WebContents* web_contents = GetWebContents();

  // Check to see if there is a single service worker controlling this page
  // and the manifest's start url.
  content::StoragePartition* storage_partition =
      content::BrowserContext::GetStoragePartition(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()),
          web_contents->GetSiteInstance());
  DCHECK(storage_partition);

  storage_partition->GetServiceWorkerContext()->CheckHasServiceWorker(
      web_contents->GetLastCommittedURL(), manifest().start_url,
      base::Bind(&InstallableManager::OnDidCheckHasServiceWorker,
                 weak_factory_.GetWeakPtr()));
}

void InstallableManager::OnDidCheckHasServiceWorker(bool has_service_worker) {
  if (!GetWebContents())
    return;

  if (has_service_worker) {
    installable_->installable = true;
  } else {
    installable_->installable = false;
    installable_->error = NO_MATCHING_SERVICE_WORKER;
  }

  installable_->fetched = true;
  WorkOnTask();
}

void InstallableManager::CheckAndFetchBestIcon() {
  DCHECK(!manifest().IsEmpty());
  DCHECK(!tasks_.empty());

  const InstallableParams& params = tasks_[0].first;
  IconProperty& icon = GetIcon(params);
  icon.fetched = true;

  GURL icon_url = ManifestIconSelector::FindBestMatchingIcon(
      manifest().icons, params.ideal_icon_size_in_px,
      params.minimum_icon_size_in_px);

  if (icon_url.is_empty()) {
    icon.error = NO_ACCEPTABLE_ICON;
  } else {
    bool can_download_icon = ManifestIconDownloader::Download(
        GetWebContents(), icon_url, params.ideal_icon_size_in_px,
        params.minimum_icon_size_in_px,
        base::Bind(&InstallableManager::OnAppIconFetched,
                   weak_factory_.GetWeakPtr(), icon_url));
    if (can_download_icon)
      return;
    icon.error = CANNOT_DOWNLOAD_ICON;
  }

  WorkOnTask();
}

void InstallableManager::OnAppIconFetched(const GURL icon_url,
                                          const SkBitmap& bitmap) {
  DCHECK(!tasks_.empty());
  const InstallableParams& params = tasks_[0].first;
  IconProperty& icon = GetIcon(params);

  if (!GetWebContents())
    return;

  if (bitmap.drawsNothing()) {
    icon.error = NO_ICON_AVAILABLE;
  } else {
    icon.url = icon_url;
    icon.icon.reset(new SkBitmap(bitmap));
  }

  WorkOnTask();
}

void InstallableManager::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (handle->IsInMainFrame() && handle->HasCommitted() &&
      !handle->IsSamePage()) {
    Reset();
  }
}

void InstallableManager::WebContentsDestroyed() {
  Reset();
  Observe(nullptr);
}

const GURL& InstallableManager::manifest_url() const {
  return manifest_->url;
}

const content::Manifest& InstallableManager::manifest() const {
  return manifest_->manifest;
}

bool InstallableManager::is_installable() const {
  return installable_->installable;
}
