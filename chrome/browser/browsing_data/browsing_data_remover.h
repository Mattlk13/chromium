// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_H_

#include <stdint.h>

#include <queue>
#include <set>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browsing_data/browsing_data_remover_delegate.h"
#include "chrome/common/features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ppapi/features/features.h"
#include "storage/common/quota/quota_types.h"
#include "url/gurl.h"

class BrowsingDataFilterBuilder;
class BrowsingDataFlashLSOHelper;
class BrowsingDataRemoverFactory;

namespace content {
class BrowserContext;
class PluginDataRemover;
class StoragePartition;
}

////////////////////////////////////////////////////////////////////////////////
// BrowsingDataRemover is responsible for removing data related to browsing:
// visits in url database, downloads, cookies ...
//
//  USAGE:
//
//  0. Instantiation.
//
//       BrowsingDataRemover remover =
//           BrowsingDataRemoverFactory::GetForBrowserContext(browser_context);
//
//  1. No observer.
//
//       remover->Remove(base::Time(), base::Time::Max(), REMOVE_COOKIES, ALL);
//
//  2. Using an observer to report when one's own removal task is finished.
//
//       class CookiesDeleter : public BrowsingDataRemover::Observer {
//         CookiesDeleter() { remover->AddObserver(this); }
//         ~CookiesDeleter() { remover->RemoveObserver(this); }
//
//         void DeleteCookies() {
//           remover->RemoveAndReply(base::Time(), base::Time::Max(),
//                                   REMOVE_COOKIES, ALL, this);
//         }
//
//         void OnBrowsingDataRemoverDone() {
//           LOG(INFO) << "Cookies were deleted.";
//         }
//       }
//
////////////////////////////////////////////////////////////////////////////////

class BrowsingDataRemover : public KeyedService {
 public:
  // Mask used for Remove.
  enum RemoveDataMask {
    REMOVE_APPCACHE = 1 << 0,
    REMOVE_CACHE = 1 << 1,
    REMOVE_COOKIES = 1 << 2,
    REMOVE_DOWNLOADS = 1 << 3,
    REMOVE_FILE_SYSTEMS = 1 << 4,
    REMOVE_FORM_DATA = 1 << 5,
    // In addition to visits, REMOVE_HISTORY removes keywords, last session and
    // passwords UI statistics.
    REMOVE_HISTORY = 1 << 6,
    REMOVE_INDEXEDDB = 1 << 7,
    REMOVE_LOCAL_STORAGE = 1 << 8,
    REMOVE_PLUGIN_DATA = 1 << 9,
    REMOVE_PASSWORDS = 1 << 10,
    REMOVE_WEBSQL = 1 << 11,
    REMOVE_CHANNEL_IDS = 1 << 12,
    REMOVE_MEDIA_LICENSES = 1 << 13,
    REMOVE_SERVICE_WORKERS = 1 << 14,
    REMOVE_SITE_USAGE_DATA = 1 << 15,
    // REMOVE_NOCHECKS intentionally does not check if the browser context is
    // prohibited from deleting history or downloads.
    REMOVE_NOCHECKS = 1 << 16,
    REMOVE_CACHE_STORAGE = 1 << 17,
#if BUILDFLAG(ANDROID_JAVA_UI)
    REMOVE_WEBAPP_DATA = 1 << 18,
#endif
    REMOVE_DURABLE_PERMISSION = 1 << 19,

    // The following flag is used only in tests. In normal usage, hosted app
    // data is controlled by the REMOVE_COOKIES flag, applied to the
    // protected-web origin.
    REMOVE_HOSTED_APP_DATA_TESTONLY = 1 << 31,

    // "Site data" includes cookies, appcache, file systems, indexedDBs, local
    // storage, webSQL, service workers, cache storage, plugin data, web app
    // data (on Android) and statistics about passwords.
    REMOVE_SITE_DATA = REMOVE_APPCACHE | REMOVE_COOKIES | REMOVE_FILE_SYSTEMS |
                       REMOVE_INDEXEDDB |
                       REMOVE_LOCAL_STORAGE |
                       REMOVE_PLUGIN_DATA |
                       REMOVE_SERVICE_WORKERS |
                       REMOVE_CACHE_STORAGE |
                       REMOVE_WEBSQL |
                       REMOVE_CHANNEL_IDS |
#if BUILDFLAG(ANDROID_JAVA_UI)
                       REMOVE_WEBAPP_DATA |
#endif
                       REMOVE_SITE_USAGE_DATA |
                       REMOVE_DURABLE_PERMISSION,

    // Datatypes protected by Important Sites.
    IMPORTANT_SITES_DATATYPES = REMOVE_SITE_DATA |
                                REMOVE_CACHE,

    // Datatypes that can be deleted partially per URL / origin / domain,
    // whichever makes sense.
    FILTERABLE_DATATYPES = REMOVE_SITE_DATA |
                           REMOVE_CACHE |
                           REMOVE_DOWNLOADS,

    // Includes all the available remove options. Meant to be used by clients
    // that wish to wipe as much data as possible from a Profile, to make it
    // look like a new Profile.
    REMOVE_ALL = REMOVE_SITE_DATA | REMOVE_CACHE | REMOVE_DOWNLOADS |
                 REMOVE_FORM_DATA |
                 REMOVE_HISTORY |
                 REMOVE_PASSWORDS |
                 REMOVE_MEDIA_LICENSES,

    // Includes all available remove options. Meant to be used when the Profile
    // is scheduled to be deleted, and all possible data should be wiped from
    // disk as soon as possible.
    REMOVE_WIPE_PROFILE = REMOVE_ALL | REMOVE_NOCHECKS,
  };

  // Important sites protect a small set of sites from the deletion of certain
  // datatypes. Therefore, those datatypes must be filterable by
  // url/origin/domain.
  static_assert(0 == (IMPORTANT_SITES_DATATYPES & ~FILTERABLE_DATATYPES),
                "All important sites datatypes must be filterable.");

  // A helper enum to report the deletion of cookies and/or cache. Do not
  // reorder the entries, as this enum is passed to UMA.
  enum CookieOrCacheDeletionChoice {
    NEITHER_COOKIES_NOR_CACHE,
    ONLY_COOKIES,
    ONLY_CACHE,
    BOTH_COOKIES_AND_CACHE,
    MAX_CHOICE_VALUE
  };

  // Observer is notified when its own removal task is done.
  class Observer {
   public:
    // Called when a removal task is finished. Note that every removal task can
    // only have one observer attached to it, and only that one is called.
    virtual void OnBrowsingDataRemoverDone() = 0;

   protected:
    virtual ~Observer() {}
  };

  // The completion inhibitor can artificially delay completion of the browsing
  // data removal process. It is used during testing to simulate scenarios in
  // which the deletion stalls or takes a very long time.
  class CompletionInhibitor {
   public:
    // Invoked when a |remover| is just about to complete clearing browser data,
    // and will be prevented from completing until after the callback
    // |continue_to_completion| is run.
    virtual void OnBrowsingDataRemoverWouldComplete(
        BrowsingDataRemover* remover,
        const base::Closure& continue_to_completion) = 0;

   protected:
    virtual ~CompletionInhibitor() {}
  };

  // Used to track the deletion of a single data storage backend.
  class SubTask {
   public:
    // Creates a SubTask that calls |forward_callback| when completed.
    // |forward_callback| is only kept as a reference and must outlive SubTask.
    explicit SubTask(const base::Closure& forward_callback);
    ~SubTask();

    // Indicate that the task is in progress and we're waiting.
    void Start();

    // Returns a callback that should be called to indicate that the task
    // has been finished.
    base::Closure GetCompletionCallback();

    // Whether the task is still in progress.
    bool is_pending() const { return is_pending_; }

   private:
    void CompletionCallback();

    bool is_pending_;
    const base::Closure& forward_callback_;
    base::WeakPtrFactory<SubTask> weak_ptr_factory_;
  };

  // Is the BrowsingDataRemover currently in the process of removing data?
  bool is_removing() { return is_removing_; }

  // Sets a CompletionInhibitor, which will be notified each time an instance is
  // about to complete a browsing data removal process, and will be able to
  // artificially delay the completion.
  // TODO(crbug.com/483528): Make this non-static.
  static void set_completion_inhibitor_for_testing(
      CompletionInhibitor* inhibitor) {
    completion_inhibitor_ = inhibitor;
  }

  // Called by the embedder to provide the delegate that will take care of
  // deleting embedder-specific data.
  void set_embedder_delegate(
      std::unique_ptr<BrowsingDataRemoverDelegate> embedder_delegate) {
    embedder_delegate_ = std::move(embedder_delegate);
  }

  BrowsingDataRemoverDelegate* get_embedder_delegate() const {
    return embedder_delegate_.get();
  }

  // Removes browsing data within the given |time_range|, with datatypes being
  // specified by |remove_mask| and origin types by |origin_type_mask|.
  void Remove(const base::Time& delete_begin,
              const base::Time& delete_end,
              int remove_mask,
              int origin_type_mask);

  // A version of the above that in addition informs the |observer| when the
  // removal task is finished.
  void RemoveAndReply(const base::Time& delete_begin,
                      const base::Time& delete_end,
                      int remove_mask,
                      int origin_type_mask,
                      Observer* observer);

  // Like Remove(), but in case of URL-keyed only removes data whose URL match
  // |filter_builder| (e.g. are on certain origin or domain).
  // RemoveWithFilter() currently only works with FILTERABLE_DATATYPES.
  void RemoveWithFilter(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      int origin_type_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder);

  // A version of the above that in addition informs the |observer| when the
  // removal task is finished.
  void RemoveWithFilterAndReply(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      int origin_type_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
      Observer* observer);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Used for testing.
  void OverrideStoragePartitionForTesting(
      content::StoragePartition* storage_partition);

#if BUILDFLAG(ENABLE_PLUGINS)
  void OverrideFlashLSOHelperForTesting(
      scoped_refptr<BrowsingDataFlashLSOHelper> flash_lso_helper);
#endif

  // Parameters of the last call are exposed to be used by tests. Removal and
  // origin type masks equal to -1 mean that no removal has ever been executed.
  // TODO(msramek): If other consumers than tests are interested in this,
  // consider returning them in OnBrowsingDataRemoverDone() callback.
  const base::Time& GetLastUsedBeginTime();
  const base::Time& GetLastUsedEndTime();
  int GetLastUsedRemovalMask();
  int GetLastUsedOriginTypeMask();

 protected:
  // Use BrowsingDataRemoverFactory::GetForBrowserContext to get an instance of
  // this class. The constructor is protected so that the class is mockable.
  BrowsingDataRemover(content::BrowserContext* browser_context);
  ~BrowsingDataRemover() override;

  // A common reduction of all public Remove[WithFilter][AndReply] methods.
  virtual void RemoveInternal(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      int origin_type_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
      Observer* observer);

 private:
  // Testing the private RemovalTask.
  FRIEND_TEST_ALL_PREFIXES(BrowsingDataRemoverTest, MultipleTasks);

  // The BrowsingDataRemover tests need to be able to access the implementation
  // of Remove(), as it exposes details that aren't yet available in the public
  // API. As soon as those details are exposed via new methods, this should be
  // removed.
  //
  // TODO(mkwst): See http://crbug.com/113621
  friend class BrowsingDataRemoverTest;

  friend class BrowsingDataRemoverFactory;

  // Represents a single removal task. Contains all parameters needed to execute
  // it and a pointer to the observer that added it.
  struct RemovalTask {
    RemovalTask(const base::Time& delete_begin,
                const base::Time& delete_end,
                int remove_mask,
                int origin_type_mask,
                std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
                Observer* observer);
    ~RemovalTask();

    base::Time delete_begin;
    base::Time delete_end;
    int remove_mask;
    int origin_type_mask;
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder;
    Observer* observer;
  };

  void Shutdown() override;

  // Setter for |is_removing_|; DCHECKs that we can only start removing if we're
  // not already removing, and vice-versa.
  void SetRemoving(bool is_removing);

#if BUILDFLAG(ENABLE_PLUGINS)
  // Called when plugin data has been cleared. Invokes NotifyIfDone.
  void OnWaitableEventSignaled(base::WaitableEvent* waitable_event);

  // Called when the list of |sites| storing Flash LSO cookies is fetched.
  void OnSitesWithFlashDataFetched(
      base::Callback<bool(const std::string&)> plugin_filter,
      const std::vector<std::string>& sites);

  // Indicates that LSO cookies for one website have been deleted.
  void OnFlashDataDeleted();
#endif

  // Executes the next removal task. Called after the previous task was finished
  // or directly from Remove() if the task queue was empty.
  void RunNextTask();

  // Removes the specified items related to browsing for a specific host. If the
  // provided |remove_url| is empty, data is removed for all origins; otherwise,
  // it is restricted by the origin filter origin (where implemented yet). The
  // |origin_type_mask| parameter defines the set of origins from which data
  // should be removed (protected, unprotected, or both).
  // TODO(ttr314): Remove "(where implemented yet)" constraint above once
  // crbug.com/113621 is done.
  // TODO(crbug.com/589586): Support all backends w/ origin filter.
  void RemoveImpl(const base::Time& delete_begin,
                  const base::Time& delete_end,
                  int remove_mask,
                  const BrowsingDataFilterBuilder& filter_builder,
                  int origin_type_mask);

  // Notifies observers and transitions to the idle state.
  void Notify();

  // Checks if we are all done, and if so, calls Notify().
  void NotifyIfDone();

  // Returns true if we're all done.
  bool AllDone();

  // The browser context we're to remove from.
  content::BrowserContext* browser_context_;

  // A delegate to delete the embedder-specific data.
  std::unique_ptr<BrowsingDataRemoverDelegate> embedder_delegate_;

  // Start time to delete from.
  base::Time delete_begin_;

  // End time to delete to.
  base::Time delete_end_;

  // The removal mask for the current removal operation.
  int remove_mask_ = 0;

  // From which types of origins should we remove data?
  int origin_type_mask_ = 0;

  // True if Remove has been invoked.
  bool is_removing_;

  // Removal tasks to be processed.
  std::queue<RemovalTask> task_queue_;

  // If non-NULL, the |completion_inhibitor_| is notified each time an instance
  // is about to complete a browsing data removal process, and has the ability
  // to artificially delay completion. Used for testing.
  static CompletionInhibitor* completion_inhibitor_;

#if BUILDFLAG(ENABLE_PLUGINS)
  // Used to delete plugin data.
  std::unique_ptr<content::PluginDataRemover> plugin_data_remover_;
  base::WaitableEventWatcher watcher_;

  // Used for per-site plugin data deletion.
  scoped_refptr<BrowsingDataFlashLSOHelper> flash_lso_helper_;
#endif

  // A callback to NotifyIfDone() used by SubTasks instances.
  const base::Closure sub_task_forward_callback_;

  // Keeping track of various subtasks to be completed.
  // These may only be accessed from UI thread in order to avoid races!
  SubTask synchronous_clear_operations_;
  SubTask clear_embedder_data_;
  SubTask clear_cache_;
  SubTask clear_channel_ids_;
  SubTask clear_http_auth_cache_;
  SubTask clear_storage_partition_data_;
  // Counts the number of plugin data tasks. Should be the number of LSO cookies
  // to be deleted, or 1 while we're fetching LSO cookies or deleting in bulk.
  int clear_plugin_data_count_ = 0;

  // Observers of the global state and individual tasks.
  base::ObserverList<Observer, true> observer_list_;

  // We do not own this.
  content::StoragePartition* storage_partition_for_testing_ = nullptr;

  base::WeakPtrFactory<BrowsingDataRemover> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemover);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_H_
