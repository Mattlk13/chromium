// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/chrome_browsing_data_remover_delegate.h"

#include <set>
#include <string>
#include <utility>

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_filter_builder.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/registrable_domain_filter_builder.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/media/media_device_id_salt.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service_factory.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/previews/previews_service.h"
#include "chrome/browser/previews/previews_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"
#include "components/domain_reliability/service.h"
#include "components/history/core/browser/history_service.h"
#include "components/nacl/browser/nacl_browser.h"
#include "components/nacl/browser/pnacl_host.h"
#include "components/ntp_snippets/bookmarks/bookmark_last_visit_utils.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/omnibox/browser/omnibox_pref_names.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/prefs/pref_service.h"
#include "components/previews/core/previews_ui_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/core/tab_restore_service.h"
#include "content/public/browser/user_metrics.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"
#include "chrome/browser/android/webapps/webapp_registry.h"
#include "chrome/browser/precache/precache_manager_factory.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/precache/content/precache_manager.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "extensions/browser/extension_prefs.h"
#endif

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chromeos/attestation/attestation_constants.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "components/user_manager/user.h"
#endif

#if BUILDFLAG(ENABLE_WEBRTC)
#include "chrome/browser/media/webrtc/webrtc_log_list.h"
#include "chrome/browser/media/webrtc/webrtc_log_util.h"
#endif

using base::UserMetricsAction;
using content::BrowserContext;
using content::BrowserThread;

namespace {

void UIThreadTrampolineHelper(const base::Closure& callback) {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, callback);
}

// Convenience method to create a callback that can be run on any thread and
// will post the given |callback| back to the UI thread.
base::Closure UIThreadTrampoline(const base::Closure& callback) {
  // We could directly bind &BrowserThread::PostTask, but that would require
  // evaluating FROM_HERE when this method is called, as opposed to when the
  // task is actually posted.
  return base::Bind(&UIThreadTrampolineHelper, callback);
}

template <typename T>
void IgnoreArgumentHelper(const base::Closure& callback, T unused_argument) {
  callback.Run();
}

// Another convenience method to turn a callback without arguments into one that
// accepts (and ignores) a single argument.
template <typename T>
base::Callback<void(T)> IgnoreArgument(const base::Closure& callback) {
  return base::Bind(&IgnoreArgumentHelper<T>, callback);
}

bool ForwardPrimaryPatternCallback(
    const base::Callback<bool(const ContentSettingsPattern&)> predicate,
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern) {
  return predicate.Run(primary_pattern);
}

#if !defined(DISABLE_NACL)
void ClearNaClCacheOnIOThread(const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  nacl::NaClBrowser::GetInstance()->ClearValidationCache(callback);
}

void ClearPnaclCacheOnIOThread(base::Time begin,
                               base::Time end,
                               const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  pnacl::PnaclHost::GetInstance()->ClearTranslationCacheEntriesBetween(
      begin, end, callback);
}
#endif

void ClearCookiesOnIOThread(base::Time delete_begin,
                            base::Time delete_end,
                            net::URLRequestContextGetter* rq_context,
                            const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::CookieStore* cookie_store =
      rq_context->GetURLRequestContext()->cookie_store();
  cookie_store->DeleteAllCreatedBetweenAsync(delete_begin, delete_end,
                                             IgnoreArgument<int>(callback));
}

void ClearCookiesWithPredicateOnIOThread(
    base::Time delete_begin,
    base::Time delete_end,
    net::CookieStore::CookiePredicate predicate,
    net::URLRequestContextGetter* rq_context,
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::CookieStore* cookie_store =
      rq_context->GetURLRequestContext()->cookie_store();
  cookie_store->DeleteAllCreatedBetweenWithPredicateAsync(
      delete_begin, delete_end, predicate, IgnoreArgument<int>(callback));
}

void ClearNetworkPredictorOnIOThread(chrome_browser_net::Predictor* predictor) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(predictor);

  predictor->DiscardInitialNavigationHistory();
  predictor->DiscardAllResults();
}

void ClearHostnameResolutionCacheOnIOThread(
    IOThread* io_thread,
    base::Callback<bool(const std::string&)> host_filter) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  io_thread->ClearHostCache(host_filter);
}

}  // namespace

ChromeBrowsingDataRemoverDelegate::ChromeBrowsingDataRemoverDelegate(
    BrowserContext* browser_context)
    : profile_(Profile::FromBrowserContext(browser_context)),
      sub_task_forward_callback_(
          base::Bind(&ChromeBrowsingDataRemoverDelegate::NotifyIfDone,
                     base::Unretained(this))),
      synchronous_clear_operations_(sub_task_forward_callback_),
      clear_autofill_origin_urls_(sub_task_forward_callback_),
      clear_flash_content_licenses_(sub_task_forward_callback_),
      clear_domain_reliability_monitor_(sub_task_forward_callback_),
      clear_form_(sub_task_forward_callback_),
      clear_history_(sub_task_forward_callback_),
      clear_keyword_data_(sub_task_forward_callback_),
#if !defined(DISABLE_NACL)
      clear_nacl_cache_(sub_task_forward_callback_),
      clear_pnacl_cache_(sub_task_forward_callback_),
#endif
      clear_hostname_resolution_cache_(sub_task_forward_callback_),
      clear_network_predictor_(sub_task_forward_callback_),
      clear_networking_history_(sub_task_forward_callback_),
      clear_passwords_(sub_task_forward_callback_),
      clear_passwords_stats_(sub_task_forward_callback_),
      clear_platform_keys_(sub_task_forward_callback_),
#if BUILDFLAG(ANDROID_JAVA_UI)
      clear_precache_history_(sub_task_forward_callback_),
      clear_offline_page_data_(sub_task_forward_callback_),
#endif
#if BUILDFLAG(ENABLE_WEBRTC)
      clear_webrtc_logs_(sub_task_forward_callback_),
#endif
      clear_auto_sign_in_(sub_task_forward_callback_),
#if BUILDFLAG(ANDROID_JAVA_UI)
    webapp_registry_(new WebappRegistry()),
#endif
    weak_ptr_factory_(this) {}

ChromeBrowsingDataRemoverDelegate::~ChromeBrowsingDataRemoverDelegate() {
  history_task_tracker_.TryCancelAll();
  template_url_sub_.reset();
}

void ChromeBrowsingDataRemoverDelegate::RemoveEmbedderData(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    int remove_mask,
    const BrowsingDataFilterBuilder& filter_builder,
    int origin_type_mask,
    const base::Closure& callback) {
  //////////////////////////////////////////////////////////////////////////////
  // INITIALIZATION
  synchronous_clear_operations_.Start();
  callback_ = callback;

  delete_begin_ = delete_begin;
  delete_end_ = delete_end;

  base::Callback<bool(const GURL& url)> filter =
      filter_builder.BuildGeneralFilter();
  base::Callback<bool(const ContentSettingsPattern& url)> same_pattern_filter =
      filter_builder.BuildWebsiteSettingsPatternMatchesFilter();

  // Some backends support a filter that |is_null()| to make complete deletion
  // more efficient.
  base::Callback<bool(const GURL&)> nullable_filter =
      filter_builder.IsEmptyBlacklist() ? base::Callback<bool(const GURL&)>()
                                        : filter;

  // Managed devices and supervised users can have restrictions on history
  // deletion.
  PrefService* prefs = profile_->GetPrefs();
  bool may_delete_history = prefs->GetBoolean(
      prefs::kAllowDeletingBrowserHistory);

  //////////////////////////////////////////////////////////////////////////////
  // REMOVE_HISTORY
  if ((remove_mask & BrowsingDataRemover::REMOVE_HISTORY) &&
      may_delete_history) {
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);
    if (history_service) {
      // TODO(dmurph): Support all backends with filter (crbug.com/113621).
      content::RecordAction(UserMetricsAction("ClearBrowsingData_History"));
      clear_history_.Start();
      history_service->ExpireLocalAndRemoteHistoryBetween(
          WebHistoryServiceFactory::GetForProfile(profile_), std::set<GURL>(),
          delete_begin_, delete_end_,
          clear_history_.GetCompletionCallback(),
          &history_task_tracker_);
    }

    // Currently, ContentSuggestionService instance exists only on Android.
    ntp_snippets::ContentSuggestionsService* content_suggestions_service =
        ContentSuggestionsServiceFactory::GetForProfileIfExists(profile_);
    if (content_suggestions_service) {
      content_suggestions_service->ClearHistory(delete_begin_, delete_end_,
                                                filter);
    }

    // Remove the last visit dates meta-data from the bookmark model.
    bookmarks::BookmarkModel* bookmark_model =
        BookmarkModelFactory::GetForBrowserContext(profile_);
    if (bookmark_model) {
      ntp_snippets::RemoveLastVisitedDatesBetween(delete_begin_, delete_end_,
                                                  filter, bookmark_model);
    }

#if BUILDFLAG(ENABLE_EXTENSIONS)
    // The extension activity log contains details of which websites extensions
    // were active on. It therefore indirectly stores details of websites a
    // user has visited so best clean from here as well.
    // TODO(msramek): Support all backends with filter (crbug.com/589586).
    extensions::ActivityLog::GetInstance(profile_)->RemoveURLs(
        std::set<GURL>());

    // Clear launch times as they are a form of history.
    // BrowsingDataFilterBuilder currently doesn't support extension origins.
    // Therefore, clearing history for a small set of origins (WHITELIST) should
    // never delete any extension launch times, while clearing for almost all
    // origins (BLACKLIST) should always delete all of extension launch times.
    if (filter_builder.mode() == BrowsingDataFilterBuilder::BLACKLIST) {
      extensions::ExtensionPrefs* extension_prefs =
          extensions::ExtensionPrefs::Get(profile_);
      extension_prefs->ClearLastLaunchTimes();
    }
#endif

    // Need to clear the host cache and accumulated speculative data, as it also
    // reveals some history. We have no mechanism to track when these items were
    // created, so we'll not honor the time range.
    // TODO(msramek): We can use the plugin filter here because plugins, same
    // as the hostname resolution cache, key their entries by hostname. Rename
    // BuildPluginFilter() to something more general to reflect this use.
    if (g_browser_process->io_thread()) {
      clear_hostname_resolution_cache_.Start();
      BrowserThread::PostTaskAndReply(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&ClearHostnameResolutionCacheOnIOThread,
                     g_browser_process->io_thread(),
                     filter_builder.BuildPluginFilter()),
          clear_hostname_resolution_cache_.GetCompletionCallback());
    }
    if (profile_->GetNetworkPredictor()) {
      // TODO(dmurph): Support all backends with filter (crbug.com/113621).
      clear_network_predictor_.Start();
      BrowserThread::PostTaskAndReply(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&ClearNetworkPredictorOnIOThread,
                     profile_->GetNetworkPredictor()),
          clear_network_predictor_.GetCompletionCallback());
      profile_->GetNetworkPredictor()->ClearPrefsOnUIThread();
    }

    // As part of history deletion we also delete the auto-generated keywords.
    TemplateURLService* keywords_model =
        TemplateURLServiceFactory::GetForProfile(profile_);

    if (keywords_model && !keywords_model->loaded()) {
      // TODO(msramek): Store filters from the currently executed task on the
      // object to avoid having to copy them to callback methods.
      template_url_sub_ = keywords_model->RegisterOnLoadedCallback(
          base::Bind(&ChromeBrowsingDataRemoverDelegate::OnKeywordsLoaded,
                     weak_ptr_factory_.GetWeakPtr(), filter));
      keywords_model->Load();
      clear_keyword_data_.Start();
    } else if (keywords_model) {
      keywords_model->RemoveAutoGeneratedForUrlsBetween(filter, delete_begin_,
                                                        delete_end_);
    }

    // The PrerenderManager keeps history of prerendered pages, so clear that.
    // It also may have a prerendered page. If so, the page could be
    // considered to have a small amount of historical information, so delete
    // it, too.
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForBrowserContext(profile_);
    if (prerender_manager) {
      // TODO(dmurph): Support all backends with filter (crbug.com/113621).
      prerender_manager->ClearData(
          prerender::PrerenderManager::CLEAR_PRERENDER_CONTENTS |
          prerender::PrerenderManager::CLEAR_PRERENDER_HISTORY);
    }

    // If the caller is removing history for all hosts, then clear ancillary
    // historical information.
    if (filter_builder.IsEmptyBlacklist()) {
      // We also delete the list of recently closed tabs. Since these expire,
      // they can't be more than a day old, so we can simply clear them all.
      sessions::TabRestoreService* tab_service =
          TabRestoreServiceFactory::GetForProfile(profile_);
      if (tab_service) {
        tab_service->ClearEntries();
        tab_service->DeleteLastSession();
      }

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
      // We also delete the last session when we delete the history.
      SessionService* session_service =
          SessionServiceFactory::GetForProfile(profile_);
      if (session_service)
        session_service->DeleteLastSession();
#endif
    }

    // The saved Autofill profiles and credit cards can include the origin from
    // which these profiles and credit cards were learned.  These are a form of
    // history, so clear them as well.
    // TODO(dmurph): Support all backends with filter (crbug.com/113621).
    scoped_refptr<autofill::AutofillWebDataService> web_data_service =
        WebDataServiceFactory::GetAutofillWebDataForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);
    if (web_data_service.get()) {
      clear_autofill_origin_urls_.Start();
      web_data_service->RemoveOriginURLsModifiedBetween(
          delete_begin_, delete_end_);
      // The above calls are done on the UI thread but do their work on the DB
      // thread. So wait for it.
      BrowserThread::PostTaskAndReply(
          BrowserThread::DB, FROM_HERE, base::Bind(&base::DoNothing),
          clear_autofill_origin_urls_.GetCompletionCallback());

      autofill::PersonalDataManager* data_manager =
          autofill::PersonalDataManagerFactory::GetForProfile(profile_);
      if (data_manager)
        data_manager->Refresh();
    }

#if BUILDFLAG(ENABLE_WEBRTC)
    clear_webrtc_logs_.Start();
    BrowserThread::PostTaskAndReply(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(
            &WebRtcLogUtil::DeleteOldAndRecentWebRtcLogFiles,
            WebRtcLogList::GetWebRtcLogDirectoryForProfile(profile_->GetPath()),
            delete_begin_),
        clear_webrtc_logs_.GetCompletionCallback());
#endif

#if BUILDFLAG(ANDROID_JAVA_UI)
    precache::PrecacheManager* precache_manager =
        precache::PrecacheManagerFactory::GetForBrowserContext(profile_);
    // |precache_manager| could be nullptr if the profile is off the record.
    if (!precache_manager) {
      clear_precache_history_.Start();
      precache_manager->ClearHistory();
      // The above calls are done on the UI thread but do their work on the DB
      // thread. So wait for it.
      BrowserThread::PostTaskAndReply(
          BrowserThread::DB, FROM_HERE, base::Bind(&base::DoNothing),
          clear_precache_history_.GetCompletionCallback());
    }

    // Clear the history information (last launch time and origin URL) of any
    // registered webapps.
    webapp_registry_->ClearWebappHistoryForUrls(filter);
#endif

    data_reduction_proxy::DataReductionProxySettings*
        data_reduction_proxy_settings =
            DataReductionProxyChromeSettingsFactory::GetForBrowserContext(
                profile_);
    // |data_reduction_proxy_settings| is null if |profile_| is off the record.
    if (data_reduction_proxy_settings) {
      data_reduction_proxy::DataReductionProxyService*
          data_reduction_proxy_service =
              data_reduction_proxy_settings->data_reduction_proxy_service();
      if (data_reduction_proxy_service) {
        data_reduction_proxy_service->compression_stats()
            ->DeleteBrowsingHistory(delete_begin_, delete_end_);
      }
    }

    // |previews_service| is null if |profile_| is off the record.
    PreviewsService* previews_service =
        PreviewsServiceFactory::GetForProfile(profile_);
    if (previews_service && previews_service->previews_ui_service()) {
      previews_service->previews_ui_service()->ClearBlackList(delete_begin_,
                                                              delete_end_);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // REMOVE_DOWNLOADS
  if ((remove_mask & BrowsingDataRemover::REMOVE_DOWNLOADS) &&
      may_delete_history) {
    DownloadPrefs* download_prefs = DownloadPrefs::FromDownloadManager(
        BrowserContext::GetDownloadManager(profile_));
    download_prefs->SetSaveFilePath(download_prefs->DownloadPath());
  }

  //////////////////////////////////////////////////////////////////////////////
  // REMOVE_COOKIES
  // We ignore the REMOVE_COOKIES request if UNPROTECTED_WEB is not set,
  // so that callers who request REMOVE_SITE_DATA with PROTECTED_WEB
  // don't accidentally remove the cookies that are associated with the
  // UNPROTECTED_WEB origin. This is necessary because cookies are not separated
  // between UNPROTECTED_WEB and PROTECTED_WEB.
  if (remove_mask & BrowsingDataRemover::REMOVE_COOKIES &&
      origin_type_mask & BrowsingDataHelper::UNPROTECTED_WEB) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_Cookies"));

    // Clear the safebrowsing cookies only if time period is for "all time".  It
    // doesn't make sense to apply the time period of deleting in the last X
    // hours/days to the safebrowsing cookies since they aren't the result of
    // any user action.
    if (delete_begin_ == base::Time()) {
      safe_browsing::SafeBrowsingService* sb_service =
          g_browser_process->safe_browsing_service();
      if (sb_service) {
        scoped_refptr<net::URLRequestContextGetter> sb_context =
            sb_service->url_request_context();
        ++clear_cookies_count_;
        if (filter_builder.IsEmptyBlacklist()) {
          BrowserThread::PostTask(
              BrowserThread::IO, FROM_HERE,
              base::Bind(
                  &ClearCookiesOnIOThread, delete_begin_, delete_end_,
                  base::RetainedRef(std::move(sb_context)),
                  UIThreadTrampoline(
                      base::Bind(
                          &ChromeBrowsingDataRemoverDelegate::OnClearedCookies,
                          weak_ptr_factory_.GetWeakPtr()))));
        } else {
          BrowserThread::PostTask(
              BrowserThread::IO, FROM_HERE,
              base::Bind(
                  &ClearCookiesWithPredicateOnIOThread, delete_begin_,
                  delete_end_, filter_builder.BuildCookieFilter(),
                  base::RetainedRef(std::move(sb_context)),
                  UIThreadTrampoline(
                      base::Bind(
                          &ChromeBrowsingDataRemoverDelegate::OnClearedCookies,
                          weak_ptr_factory_.GetWeakPtr()))));
        }
      }
    }

    MediaDeviceIDSalt::Reset(profile_->GetPrefs());
  }

  //////////////////////////////////////////////////////////////////////////////
  // REMOVE_DURABLE_PERMISSION
  if (remove_mask & BrowsingDataRemover::REMOVE_DURABLE_PERMISSION) {
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->ClearSettingsForOneTypeWithPredicate(
            CONTENT_SETTINGS_TYPE_DURABLE_STORAGE,
            base::Bind(&ForwardPrimaryPatternCallback, same_pattern_filter));
  }

  //////////////////////////////////////////////////////////////////////////////
  // REMOVE_SITE_USAGE_DATA
  if (remove_mask & BrowsingDataRemover::REMOVE_SITE_USAGE_DATA) {
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->ClearSettingsForOneTypeWithPredicate(
            CONTENT_SETTINGS_TYPE_SITE_ENGAGEMENT,
            base::Bind(&ForwardPrimaryPatternCallback, same_pattern_filter));
  }

  if ((remove_mask & BrowsingDataRemover::REMOVE_SITE_USAGE_DATA) ||
      (remove_mask & BrowsingDataRemover::REMOVE_HISTORY)) {
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->ClearSettingsForOneTypeWithPredicate(
            CONTENT_SETTINGS_TYPE_APP_BANNER,
            base::Bind(&ForwardPrimaryPatternCallback, same_pattern_filter));

    PermissionDecisionAutoBlocker::RemoveCountsByUrl(profile_, filter);
  }

  //////////////////////////////////////////////////////////////////////////////
  // Password manager
  if (remove_mask & BrowsingDataRemover::REMOVE_PASSWORDS) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_Passwords"));
    password_manager::PasswordStore* password_store =
        PasswordStoreFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS).get();

    if (password_store) {
      clear_passwords_.Start();
      password_store->RemoveLoginsByURLAndTime(
          filter, delete_begin_, delete_end_,
          clear_passwords_.GetCompletionCallback());
    }
  }

  if (remove_mask & BrowsingDataRemover::REMOVE_COOKIES) {
    password_manager::PasswordStore* password_store =
        PasswordStoreFactory::GetForProfile(profile_,
                                            ServiceAccessType::EXPLICIT_ACCESS)
            .get();

    if (password_store) {
      clear_auto_sign_in_.Start();
      password_store->DisableAutoSignInForOrigins(
          filter, clear_auto_sign_in_.GetCompletionCallback());
    }
  }

  if (remove_mask & BrowsingDataRemover::REMOVE_HISTORY) {
    password_manager::PasswordStore* password_store =
        PasswordStoreFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS).get();

    if (password_store) {
      clear_passwords_stats_.Start();
      password_store->RemoveStatisticsByOriginAndTime(
          nullable_filter, delete_begin_, delete_end_,
          clear_passwords_stats_.GetCompletionCallback());
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // REMOVE_FORM_DATA
  // TODO(dmurph): Support all backends with filter (crbug.com/113621).
  if (remove_mask & BrowsingDataRemover::REMOVE_FORM_DATA) {
    content::RecordAction(UserMetricsAction("ClearBrowsingData_Autofill"));
    scoped_refptr<autofill::AutofillWebDataService> web_data_service =
        WebDataServiceFactory::GetAutofillWebDataForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS);

    if (web_data_service.get()) {
      clear_form_.Start();
      web_data_service->RemoveFormElementsAddedBetween(delete_begin_,
          delete_end_);
      web_data_service->RemoveAutofillDataModifiedBetween(
          delete_begin_, delete_end_);
      // The above calls are done on the UI thread but do their work on the DB
      // thread. So wait for it.
      BrowserThread::PostTaskAndReply(
          BrowserThread::DB, FROM_HERE, base::Bind(&base::DoNothing),
          clear_form_.GetCompletionCallback());

      autofill::PersonalDataManager* data_manager =
          autofill::PersonalDataManagerFactory::GetForProfile(profile_);
      if (data_manager)
        data_manager->Refresh();
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // REMOVE_CACHE
  if (remove_mask & BrowsingDataRemover::REMOVE_CACHE) {
#if !defined(DISABLE_NACL)
    clear_nacl_cache_.Start();

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ClearNaClCacheOnIOThread,
                   UIThreadTrampoline(
                       clear_nacl_cache_.GetCompletionCallback())));

    clear_pnacl_cache_.Start();
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &ClearPnaclCacheOnIOThread, delete_begin_, delete_end_,
            UIThreadTrampoline(clear_pnacl_cache_.GetCompletionCallback())));
#endif

    // The PrerenderManager may have a page actively being prerendered, which
    // is essentially a preemptively cached page.
    prerender::PrerenderManager* prerender_manager =
        prerender::PrerenderManagerFactory::GetForBrowserContext(profile_);
    if (prerender_manager) {
      prerender_manager->ClearData(
          prerender::PrerenderManager::CLEAR_PRERENDER_CONTENTS);
    }

    // When clearing cache, wipe accumulated network related data
    // (TransportSecurityState and HttpServerPropertiesManager data).
    clear_networking_history_.Start();
    profile_->ClearNetworkingHistorySince(
        delete_begin_,
        clear_networking_history_.GetCompletionCallback());

    ntp_snippets::ContentSuggestionsService* content_suggestions_service =
        ContentSuggestionsServiceFactory::GetForProfileIfExists(profile_);
    if (content_suggestions_service)
      content_suggestions_service->ClearAllCachedSuggestions();

    // |ui_nqe_service| may be null if |profile_| is not a regular profile.
    UINetworkQualityEstimatorService* ui_nqe_service =
        UINetworkQualityEstimatorServiceFactory::GetForProfile(profile_);
    DCHECK(profile_->GetProfileType() !=
               Profile::ProfileType::REGULAR_PROFILE ||
           ui_nqe_service != nullptr);
    if (ui_nqe_service) {
      // Network Quality Estimator (NQE) stores the quality (RTT, bandwidth
      // etc.) of different networks in prefs. The stored quality is not
      // broken down by URLs or timestamps, so clearing the cache should
      // completely clear the prefs.
      ui_nqe_service->ClearPrefs();
    }

#if BUILDFLAG(ANDROID_JAVA_UI)
    // For now we're considering offline pages as cache, so if we're removing
    // cache we should remove offline pages as well.
    if ((remove_mask & BrowsingDataRemover::REMOVE_CACHE)) {
      clear_offline_page_data_.Start();
      offline_pages::OfflinePageModelFactory::GetForBrowserContext(profile_)
          ->DeleteCachedPagesByURLPredicate(
              filter,
              IgnoreArgument<offline_pages::OfflinePageModel::DeletePageResult>(
                  clear_offline_page_data_.GetCompletionCallback()));
    }
#endif
  }

  //////////////////////////////////////////////////////////////////////////////
  // REMOVE_MEDIA_LICENSES
  if (remove_mask & BrowsingDataRemover::REMOVE_MEDIA_LICENSES) {
    // TODO(jrummell): This UMA should be renamed to indicate it is for Media
    // Licenses.
    content::RecordAction(
        UserMetricsAction("ClearBrowsingData_ContentLicenses"));

#if BUILDFLAG(ENABLE_PLUGINS)
    clear_flash_content_licenses_.Start();
    if (!pepper_flash_settings_manager_.get()) {
      pepper_flash_settings_manager_.reset(
          new PepperFlashSettingsManager(this, profile_));
    }
    deauthorize_flash_content_licenses_request_id_ =
        pepper_flash_settings_manager_->DeauthorizeContentLicenses(prefs);
#if defined(OS_CHROMEOS)
    // On Chrome OS, also delete any content protection platform keys.
    const user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
    if (!user) {
      LOG(WARNING) << "Failed to find user for current profile.";
    } else {
      clear_platform_keys_.Start();
      chromeos::DBusThreadManager::Get()
          ->GetCryptohomeClient()
          ->TpmAttestationDeleteKeys(
              chromeos::attestation::KEY_USER,
              cryptohome::Identification(user->GetAccountId()),
              chromeos::attestation::kContentProtectionKeyPrefix,
              base::Bind(
                  &ChromeBrowsingDataRemoverDelegate::OnClearPlatformKeys,
                  weak_ptr_factory_.GetWeakPtr()));
    }
#endif  // defined(OS_CHROMEOS)
#endif  // BUILDFLAG(ENABLE_PLUGINS)
  }

  //////////////////////////////////////////////////////////////////////////////
  // Zero suggest.
  // Remove omnibox zero-suggest cache results. Filtering is not supported.
  // This is not a problem, as deleting more data than necessary will just cause
  // another server round-trip; no data is actually lost.
  if ((remove_mask & (BrowsingDataRemover::REMOVE_CACHE |
                      BrowsingDataRemover::REMOVE_COOKIES))) {
    prefs->SetString(omnibox::kZeroSuggestCachedResults, std::string());
  }

  //////////////////////////////////////////////////////////////////////////////
  // Domain reliability service.
  if (remove_mask & (BrowsingDataRemover::REMOVE_COOKIES |
                     BrowsingDataRemover::REMOVE_HISTORY)) {
    domain_reliability::DomainReliabilityService* service =
      domain_reliability::DomainReliabilityServiceFactory::
          GetForBrowserContext(profile_);
    if (service) {
      domain_reliability::DomainReliabilityClearMode mode;
      if (remove_mask & BrowsingDataRemover::REMOVE_COOKIES)
        mode = domain_reliability::CLEAR_CONTEXTS;
      else
        mode = domain_reliability::CLEAR_BEACONS;

      clear_domain_reliability_monitor_.Start();
      service->ClearBrowsingData(
          mode,
          filter,
          clear_domain_reliability_monitor_.GetCompletionCallback());
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // REMOVE_WEBAPP_DATA
#if BUILDFLAG(ANDROID_JAVA_UI)
  // Clear all data associated with registered webapps.
  if (remove_mask & BrowsingDataRemover::REMOVE_WEBAPP_DATA)
    webapp_registry_->UnregisterWebappsForUrls(filter);
#endif

  synchronous_clear_operations_.GetCompletionCallback().Run();
}

void ChromeBrowsingDataRemoverDelegate::NotifyIfDone() {
  if (!AllDone())
    return;

  DCHECK(!callback_.is_null());
  callback_.Run();
}

bool ChromeBrowsingDataRemoverDelegate::AllDone() {
  return !clear_cookies_count_ &&
         !synchronous_clear_operations_.is_pending() &&
         !clear_autofill_origin_urls_.is_pending() &&
         !clear_flash_content_licenses_.is_pending() &&
         !clear_domain_reliability_monitor_.is_pending() &&
         !clear_form_.is_pending() &&
         !clear_history_.is_pending() &&
         !clear_hostname_resolution_cache_.is_pending() &&
         !clear_keyword_data_.is_pending() &&
#if !defined(DISABLE_NACL)
         !clear_nacl_cache_.is_pending() &&
         !clear_pnacl_cache_.is_pending() &&
#endif
         !clear_network_predictor_.is_pending() &&
         !clear_networking_history_.is_pending() &&
         !clear_passwords_.is_pending() &&
         !clear_passwords_stats_.is_pending() &&
         !clear_platform_keys_.is_pending() &&
#if BUILDFLAG(ANDROID_JAVA_UI)
         !clear_precache_history_.is_pending() &&
         !clear_offline_page_data_.is_pending() &&
#endif
#if BUILDFLAG(ENABLE_WEBRTC)
         !clear_webrtc_logs_.is_pending() &&
#endif
         !clear_auto_sign_in_.is_pending();
}

#if BUILDFLAG(ANDROID_JAVA_UI)
void ChromeBrowsingDataRemoverDelegate::OverrideWebappRegistryForTesting(
    std::unique_ptr<WebappRegistry> webapp_registry) {
  webapp_registry_ = std::move(webapp_registry);
}
#endif

void ChromeBrowsingDataRemoverDelegate::OnKeywordsLoaded(
    base::Callback<bool(const GURL&)> url_filter) {
  // Deletes the entries from the model.
  TemplateURLService* model =
      TemplateURLServiceFactory::GetForProfile(profile_);
  model->RemoveAutoGeneratedForUrlsBetween(url_filter, delete_begin_,
                                           delete_end_);
  template_url_sub_.reset();
  clear_keyword_data_.GetCompletionCallback().Run();
}

void ChromeBrowsingDataRemoverDelegate::OnClearedCookies() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK_GT(clear_cookies_count_, 0);
  --clear_cookies_count_;
  NotifyIfDone();
}

#if BUILDFLAG(ENABLE_PLUGINS)
void ChromeBrowsingDataRemoverDelegate::
OnDeauthorizeFlashContentLicensesCompleted(
    uint32_t request_id,
    bool /* success */) {
  DCHECK_EQ(request_id, deauthorize_flash_content_licenses_request_id_);
  clear_flash_content_licenses_.GetCompletionCallback().Run();
}
#endif

#if defined(OS_CHROMEOS)
void ChromeBrowsingDataRemoverDelegate::OnClearPlatformKeys(
    chromeos::DBusMethodCallStatus call_status,
    bool result) {
  LOG_IF(ERROR, call_status != chromeos::DBUS_METHOD_CALL_SUCCESS || !result)
      << "Failed to clear platform keys.";
  clear_platform_keys_.GetCompletionCallback().Run();
}
#endif
