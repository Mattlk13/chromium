// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/prerendering_offliner.h"

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/sys_info.h"
#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/offline_pages/core/background/save_page_request.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

namespace {

bool AreThirdPartyCookiesBlocked(content::BrowserContext* browser_context) {
  return Profile::FromBrowserContext(browser_context)
      ->GetPrefs()
      ->GetBoolean(prefs::kBlockThirdPartyCookies);
}

bool IsNetworkPredictionDisabled(content::BrowserContext* browser_context) {
  return Profile::FromBrowserContext(browser_context)
             ->GetPrefs()
             ->GetInteger(prefs::kNetworkPredictionOptions) ==
         chrome_browser_net::NETWORK_PREDICTION_NEVER;
}

enum class OfflinePagesCctApiPrerenderAllowedStatus {
  PRERENDER_ALLOWED,
  THIRD_PARTY_COOKIES_DISABLED,
  NETWORK_PREDICTION_DISABLED,
};

}  // namespace

namespace offline_pages {

PrerenderingOffliner::PrerenderingOffliner(
    content::BrowserContext* browser_context,
    const OfflinerPolicy* policy,
    OfflinePageModel* offline_page_model)
    : browser_context_(browser_context),
      offline_page_model_(offline_page_model),
      pending_request_(nullptr),
      is_low_end_device_(base::SysInfo::IsLowEndDevice()),
      app_listener_(nullptr),
      weak_ptr_factory_(this) {}

PrerenderingOffliner::~PrerenderingOffliner() {}

void PrerenderingOffliner::OnLoadPageDone(
    const SavePageRequest& request,
    Offliner::RequestStatus load_status,
    content::WebContents* web_contents) {
  // Check if request is still pending receiving a callback.
  // Note: it is possible to get a loaded page, start the save operation,
  // and then get another callback from the Loader (eg, if its loaded
  // WebContents is being destroyed for some resource reclamation).
  if (!pending_request_)
    return;

  // Since we are able to stop/cancel a previous load request, we should
  // never see a callback for an older request when we have a newer one
  // pending. Crash for debug build and ignore for production build.
  DCHECK_EQ(request.request_id(), pending_request_->request_id());
  if (request.request_id() != pending_request_->request_id()) {
    DVLOG(1) << "Ignoring load callback for old request";
    return;
  }

  if (load_status == Offliner::RequestStatus::LOADED) {
    // The page has successfully loaded so now try to save the page.
    // After issuing the save request we will wait for either: the save
    // callback or a CANCELED load callback (triggered because the loaded
    // WebContents are being destroyed) - whichever callback occurs first.
    DCHECK(web_contents);
    std::unique_ptr<OfflinePageArchiver> archiver(
        new OfflinePageMHTMLArchiver(web_contents));

    OfflinePageModel::SavePageParams save_page_params;
    save_page_params.url = web_contents->GetLastCommittedURL();
    save_page_params.client_id = request.client_id();
    save_page_params.proposed_offline_id = request.request_id();
    // Pass in the original URL if it is different from the last committed URL
    // when redirects occur.
    if (save_page_params.url != request.url())
      save_page_params.original_url = request.url();

    SavePage(save_page_params, std::move(archiver),
             base::Bind(&PrerenderingOffliner::OnSavePageDone,
                        weak_ptr_factory_.GetWeakPtr(), request));
  } else {
    // Clear pending request and app listener then run completion callback.
    pending_request_.reset(nullptr);
    app_listener_.reset(nullptr);
    completion_callback_.Run(request, load_status);
  }
}

void PrerenderingOffliner::OnSavePageDone(
    const SavePageRequest& request,
    SavePageResult save_result,
    int64_t offline_id) {
  // Check if request is still pending receiving a callback.
  if (!pending_request_)
    return;

  // Also check that this completed request is same as the pending one
  // (since SavePage request is not cancel-able currently and could be old).
  if (request.request_id() != pending_request_->request_id()) {
    DVLOG(1) << "Ignoring save callback for old request";
    return;
  }

  // Clear pending request and app listener here and then inform loader we
  // are done with WebContents.
  pending_request_.reset(nullptr);
  app_listener_.reset(nullptr);
  GetOrCreateLoader()->StopLoading();

  // Determine status and run the completion callback.
  Offliner::RequestStatus save_status;
  if (save_result == SavePageResult::SUCCESS) {
    save_status = RequestStatus::SAVED;
  } else {
    // TODO(dougarnett): Consider reflecting some recommendation to retry the
    // request based on specific save error cases.
    save_status = RequestStatus::SAVE_FAILED;
  }
  completion_callback_.Run(request, save_status);
}

bool PrerenderingOffliner::LoadAndSave(const SavePageRequest& request,
                                       const CompletionCallback& callback) {
  DCHECK(!pending_request_.get());

  if (pending_request_) {
    DVLOG(1) << "Already have pending request";
    return false;
  }

  // Do not allow loading for custom tabs clients if 3rd party cookies blocked.
  // TODO(dewittj): Revise api to specify policy rather than hard code to
  // name_space.
  if (request.client_id().name_space == kCCTNamespace &&
      (AreThirdPartyCookiesBlocked(browser_context_) ||
       IsNetworkPredictionDisabled(browser_context_))) {
    DVLOG(1) << "WARNING: Unable to load when 3rd party cookies blocked or "
             << "prediction disabled";
    // Record user metrics for third party cookies being disabled or network
    // prediction being disabled.
    if (AreThirdPartyCookiesBlocked(browser_context_)) {
      UMA_HISTOGRAM_ENUMERATION(
          "OfflinePages.Background.CctApiDisableStatus",
          static_cast<int>(OfflinePagesCctApiPrerenderAllowedStatus::
                               THIRD_PARTY_COOKIES_DISABLED),
          static_cast<int>(OfflinePagesCctApiPrerenderAllowedStatus::
                               NETWORK_PREDICTION_DISABLED) +
              1);
    }
    if (IsNetworkPredictionDisabled(browser_context_)) {
      UMA_HISTOGRAM_ENUMERATION(
          "OfflinePages.Background.CctApiDisableStatus",
          static_cast<int>(OfflinePagesCctApiPrerenderAllowedStatus::
                               NETWORK_PREDICTION_DISABLED),
          static_cast<int>(OfflinePagesCctApiPrerenderAllowedStatus::
                               NETWORK_PREDICTION_DISABLED) +
              1);
    }

    return false;
  }

  // Record UMA that the prerender was allowed to proceed.
  if (request.client_id().name_space == kCCTNamespace) {
    UMA_HISTOGRAM_ENUMERATION(
        "OfflinePages.Background.CctApiDisableStatus",
        static_cast<int>(
            OfflinePagesCctApiPrerenderAllowedStatus::PRERENDER_ALLOWED),
        static_cast<int>(OfflinePagesCctApiPrerenderAllowedStatus::
                         NETWORK_PREDICTION_DISABLED) +
        1);
  }

  if (!OfflinePageModel::CanSaveURL(request.url())) {
    DVLOG(1) << "Not able to save page for requested url: " << request.url();
    return false;
  }

  // Track copy of pending request for callback handling.
  pending_request_.reset(new SavePageRequest(request));
  completion_callback_ = callback;

  // Kick off load page attempt.
  bool accepted = GetOrCreateLoader()->LoadPage(
      request.url(), base::Bind(&PrerenderingOffliner::OnLoadPageDone,
                                weak_ptr_factory_.GetWeakPtr(), request));
  if (!accepted) {
    pending_request_.reset(nullptr);
  } else {
    // Create app listener for the pending request.
    app_listener_.reset(new base::android::ApplicationStatusListener(
        base::Bind(&PrerenderingOffliner::OnApplicationStateChange,
                   weak_ptr_factory_.GetWeakPtr())));
  }

  return accepted;
}

void PrerenderingOffliner::Cancel() {
  if (pending_request_) {
    pending_request_.reset(nullptr);
    app_listener_.reset(nullptr);
    GetOrCreateLoader()->StopLoading();
    // TODO(dougarnett): Consider ability to cancel SavePage request.
  }
}

void PrerenderingOffliner::SetLoaderForTesting(
    std::unique_ptr<PrerenderingLoader> loader) {
  DCHECK(!loader_);
  loader_ = std::move(loader);
}

void PrerenderingOffliner::SetLowEndDeviceForTesting(bool is_low_end_device) {
  is_low_end_device_ = is_low_end_device;
}

void PrerenderingOffliner::SetApplicationStateForTesting(
    base::android::ApplicationState application_state) {
  OnApplicationStateChange(application_state);
}

void PrerenderingOffliner::SavePage(
    const OfflinePageModel::SavePageParams& save_page_params,
    std::unique_ptr<OfflinePageArchiver> archiver,
    const SavePageCallback& save_callback) {
  DCHECK(offline_page_model_);
  offline_page_model_->SavePage(
      save_page_params, std::move(archiver), save_callback);
}

PrerenderingLoader* PrerenderingOffliner::GetOrCreateLoader() {
  if (!loader_) {
    loader_.reset(new PrerenderingLoader(browser_context_));
  }
  return loader_.get();
}

void PrerenderingOffliner::OnApplicationStateChange(
    base::android::ApplicationState application_state) {
  if (pending_request_ && is_low_end_device_ &&
      application_state ==
          base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES) {
    DVLOG(1) << "App became active, canceling current offlining request";
    SavePageRequest* request = pending_request_.get();
    Cancel();
    completion_callback_.Run(*request,
                             Offliner::RequestStatus::FOREGROUND_CANCELED);
  }
}

}  // namespace offline_pages
