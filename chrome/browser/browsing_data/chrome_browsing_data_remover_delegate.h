// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_DELEGATE_H_
#define CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_DELEGATE_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/browsing_data/browsing_data_remover_delegate.h"
#include "chrome/common/features.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/search_engines/template_url_service.h"
#include "media/media_features.h"
#include "ppapi/features/features.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/pepper_flash_settings_manager.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_method_call_status.h"
#endif

class Profile;
class WebappRegistry;

// A delegate used by BrowsingDataRemover to delete data specific to Chrome
// as the embedder.
class ChromeBrowsingDataRemoverDelegate : public BrowsingDataRemoverDelegate
#if BUILDFLAG(ENABLE_PLUGINS)
    , public PepperFlashSettingsManager::Client
#endif
{
 public:
  ChromeBrowsingDataRemoverDelegate(content::BrowserContext* browser_context);
  ~ChromeBrowsingDataRemoverDelegate() override;

  // Removes Chrome-specific data.
  void RemoveEmbedderData(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      const BrowsingDataFilterBuilder& filter_builder,
      int origin_type_mask,
      const base::Closure& callback) override;

#if BUILDFLAG(ANDROID_JAVA_UI)
  void OverrideWebappRegistryForTesting(
      std::unique_ptr<WebappRegistry> webapp_registry);
#endif

 private:
  // If AllDone(), calls the callback provided in RemoveEmbedderData().
  void NotifyIfDone();

  // Whether there are no running deletion tasks.
  bool AllDone();

  // Callback for when TemplateURLService has finished loading. Clears the data,
  // clears the respective waiting flag, and invokes NotifyIfDone.
  void OnKeywordsLoaded(base::Callback<bool(const GURL&)> url_filter);

#if defined (OS_CHROMEOS)
  void OnClearPlatformKeys(chromeos::DBusMethodCallStatus call_status,
                           bool result);
#endif

  // Callback for when cookies have been deleted. Invokes NotifyIfDone.
  void OnClearedCookies();

#if BUILDFLAG(ENABLE_PLUGINS)
  // PepperFlashSettingsManager::Client implementation.
  void OnDeauthorizeFlashContentLicensesCompleted(uint32_t request_id,
                                                  bool success) override;
#endif

  // The profile for which the data will be deleted.
  Profile* profile_;

  // Start time to delete from.
  base::Time delete_begin_;

  // End time to delete to.
  base::Time delete_end_;

  // Completion callback to call when all data are deleted.
  base::Closure callback_;

  // A callback to NotifyIfDone() used by SubTasks instances.
  const base::Closure sub_task_forward_callback_;

  // Keeping track of various subtasks to be completed.
  // Non-zero if waiting for SafeBrowsing cookies to be cleared.
  int clear_cookies_count_ = 0;
  BrowsingDataRemover::SubTask synchronous_clear_operations_;
  BrowsingDataRemover::SubTask clear_autofill_origin_urls_;
  BrowsingDataRemover::SubTask clear_flash_content_licenses_;
  BrowsingDataRemover::SubTask clear_domain_reliability_monitor_;
  BrowsingDataRemover::SubTask clear_form_;
  BrowsingDataRemover::SubTask clear_history_;
  BrowsingDataRemover::SubTask clear_keyword_data_;
#if !defined(DISABLE_NACL)
  BrowsingDataRemover::SubTask clear_nacl_cache_;
  BrowsingDataRemover::SubTask clear_pnacl_cache_;
#endif
  BrowsingDataRemover::SubTask clear_hostname_resolution_cache_;
  BrowsingDataRemover::SubTask clear_network_predictor_;
  BrowsingDataRemover::SubTask clear_networking_history_;
  BrowsingDataRemover::SubTask clear_passwords_;
  BrowsingDataRemover::SubTask clear_passwords_stats_;
  BrowsingDataRemover::SubTask clear_platform_keys_;
#if BUILDFLAG(ANDROID_JAVA_UI)
  BrowsingDataRemover::SubTask clear_precache_history_;
  BrowsingDataRemover::SubTask clear_offline_page_data_;
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
  BrowsingDataRemover::SubTask clear_webrtc_logs_;
#endif
  BrowsingDataRemover::SubTask clear_auto_sign_in_;

#if BUILDFLAG(ENABLE_PLUGINS)
  uint32_t deauthorize_flash_content_licenses_request_id_ = 0;

  // Used to deauthorize content licenses for Pepper Flash.
  std::unique_ptr<PepperFlashSettingsManager> pepper_flash_settings_manager_;
#endif

  // Used if we need to clear history.
  base::CancelableTaskTracker history_task_tracker_;

  std::unique_ptr<TemplateURLService::Subscription> template_url_sub_;

#if BUILDFLAG(ANDROID_JAVA_UI)
  // WebappRegistry makes calls across the JNI. In unit tests, the Java side is
  // not initialised, so the registry must be mocked out.
  std::unique_ptr<WebappRegistry> webapp_registry_;
#endif

  base::WeakPtrFactory<ChromeBrowsingDataRemoverDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowsingDataRemoverDelegate);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_DELEGATE_H_
