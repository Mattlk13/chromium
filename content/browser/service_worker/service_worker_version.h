// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_

#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_request_handler.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_script_cache_map.h"
#include "content/common/content_export.h"
#include "content/common/origin_trials/trial_token_validator.h"
#include "content/common/service_worker/service_worker_event_dispatcher.mojom.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/common/service_worker/service_worker_types.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/WebKit/public/platform/modules/serviceworker/WebServiceWorkerEventResult.h"
#include "url/gurl.h"
#include "url/origin.h"

// Windows headers will redefine SendMessage.
#ifdef SendMessage
#undef SendMessage
#endif

namespace net {
class HttpResponseInfo;
}

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
class ServiceWorkerURLRequestJob;
struct ServiceWorkerClientInfo;
struct ServiceWorkerVersionInfo;

// This class corresponds to a specific version of a ServiceWorker
// script for a given pattern. When a script is upgraded, there may be
// more than one ServiceWorkerVersion "running" at a time, but only
// one of them is activated. This class connects the actual script with a
// running worker.
class CONTENT_EXPORT ServiceWorkerVersion
    : NON_EXPORTED_BASE(public base::RefCounted<ServiceWorkerVersion>),
      public EmbeddedWorkerInstance::Listener {
 public:
  using StatusCallback = base::Callback<void(ServiceWorkerStatusCode)>;

  // Current version status; some of the status (e.g. INSTALLED and ACTIVATED)
  // should be persisted unlike running status.
  enum Status {
    NEW,         // The version is just created.
    INSTALLING,  // Install event is dispatched and being handled.
    INSTALLED,   // Install event is finished and is ready to be activated.
    ACTIVATING,  // Activate event is dispatched and being handled.
    ACTIVATED,   // Activation is finished and can run as activated.
    REDUNDANT,   // The version is no longer running as activated, due to
                 // unregistration or replace.
  };

  // Behavior when a request times out.
  enum TimeoutBehavior {
    KILL_ON_TIMEOUT,     // Kill the worker if this request times out.
    CONTINUE_ON_TIMEOUT  // Keep the worker alive, only abandon the request that
                         // timed out.
  };

  // Whether the version has fetch handlers or not.
  enum class FetchHandlerExistence {
    UNKNOWN,  // This version is a new version and not installed yet.
    EXISTS,
    DOES_NOT_EXIST,
  };

  class Listener {
   public:
    virtual void OnRunningStateChanged(ServiceWorkerVersion* version) {}
    virtual void OnVersionStateChanged(ServiceWorkerVersion* version) {}
    virtual void OnDevToolsRoutingIdChanged(ServiceWorkerVersion* version) {}
    virtual void OnMainScriptHttpResponseInfoSet(
        ServiceWorkerVersion* version) {}
    virtual void OnErrorReported(ServiceWorkerVersion* version,
                                 const base::string16& error_message,
                                 int line_number,
                                 int column_number,
                                 const GURL& source_url) {}
    virtual void OnReportConsoleMessage(ServiceWorkerVersion* version,
                                        int source_identifier,
                                        int message_level,
                                        const base::string16& message,
                                        int line_number,
                                        const GURL& source_url) {}
    virtual void OnControlleeAdded(ServiceWorkerVersion* version,
                                   ServiceWorkerProviderHost* provider_host) {}
    virtual void OnControlleeRemoved(ServiceWorkerVersion* version,
                                     ServiceWorkerProviderHost* provider_host) {
    }
    virtual void OnNoControllees(ServiceWorkerVersion* version) {}
    virtual void OnNoWork(ServiceWorkerVersion* version) {}
    virtual void OnCachedMetadataUpdated(ServiceWorkerVersion* version) {}

   protected:
    virtual ~Listener() {}
  };

  ServiceWorkerVersion(ServiceWorkerRegistration* registration,
                       const GURL& script_url,
                       int64_t version_id,
                       base::WeakPtr<ServiceWorkerContextCore> context);

  int64_t version_id() const { return version_id_; }
  int64_t registration_id() const { return registration_id_; }
  const GURL& script_url() const { return script_url_; }
  const GURL& scope() const { return scope_; }
  EmbeddedWorkerStatus running_status() const {
    return embedded_worker_->status();
  }
  ServiceWorkerVersionInfo GetInfo();
  Status status() const { return status_; }

  // This status is set to EXISTS or DOES_NOT_EXIST when the install event has
  // been executed in a new version or when an installed version is loaded from
  // the storage. When a new version is not installed yet, it is  UNKNOW.
  FetchHandlerExistence fetch_handler_existence() const {
    return fetch_handler_existence_;
  }
  // This also updates |site_for_uma_| when it was Site::OTHER.
  void set_fetch_handler_existence(FetchHandlerExistence existence);

  bool should_exclude_from_uma() const { return should_exclude_from_uma_; }

  const std::vector<GURL>& foreign_fetch_scopes() const {
    return foreign_fetch_scopes_;
  }
  void set_foreign_fetch_scopes(const std::vector<GURL>& scopes) {
    foreign_fetch_scopes_ = scopes;
  }

  const std::vector<url::Origin>& foreign_fetch_origins() const {
    return foreign_fetch_origins_;
  }
  void set_foreign_fetch_origins(const std::vector<url::Origin>& origins) {
    foreign_fetch_origins_ = origins;
  }

  // Meaningful only if this version is active.
  const NavigationPreloadState& navigation_preload_state() const {
    DCHECK(status_ == ACTIVATING || status_ == ACTIVATED) << status_;
    return navigation_preload_state_;
  }
  // Only intended for use by ServiceWorkerRegistration. Generally use
  // ServiceWorkerRegistration::EnableNavigationPreload or
  // ServiceWorkerRegistration::SetNavigationPreloadHeader instead of this
  // function.
  void SetNavigationPreloadState(const NavigationPreloadState& state);

  ServiceWorkerMetrics::Site site_for_uma() const { return site_for_uma_; }

  // This sets the new status and also run status change callbacks
  // if there're any (see RegisterStatusChangeCallback).
  void SetStatus(Status status);

  // Registers status change callback. (This is for one-off observation,
  // the consumer needs to re-register if it wants to continue observing
  // status changes)
  void RegisterStatusChangeCallback(const base::Closure& callback);

  // Starts an embedded worker for this version.
  // This returns OK (success) if the worker is already running.
  // |purpose| is recorded in UMA.
  void StartWorker(ServiceWorkerMetrics::EventType purpose,
                   const StatusCallback& callback);

  // Stops an embedded worker for this version.
  // This returns OK (success) if the worker is already stopped.
  void StopWorker(const StatusCallback& callback);

  // Skips waiting and forces this version to become activated.
  void SkipWaitingFromDevTools();

  // Schedules an update to be run 'soon'.
  void ScheduleUpdate();

  // If an update is scheduled but not yet started, this resets the timer
  // delaying the start time by a 'small' amount.
  void DeferScheduledUpdate();

  // Starts an update now.
  void StartUpdate();

  // Starts the worker if it isn't already running, and calls |task| when the
  // worker is running, or |error_callback| if starting the worker failed.
  // If the worker is already running, |task| is executed synchronously (before
  // this method returns).
  // |purpose| is used for UMA.
  void RunAfterStartWorker(ServiceWorkerMetrics::EventType purpose,
                           const base::Closure& task,
                           const StatusCallback& error_callback);

  // Call this while the worker is running before dispatching an event to the
  // worker. This informs ServiceWorkerVersion about the event in progress. The
  // worker attempts to keep running until the event finishes.
  //
  // Returns a request id, which must later be passed to FinishRequest when the
  // event finished. The caller is responsible for ensuring FinishRequest is
  // called. If FinishRequest is not called the request will eventually time
  // out and the worker will be forcibly terminated.
  //
  // The |error_callback| is called if either ServiceWorkerVersion decides the
  // event is taking too long, or if for some reason the worker stops or is
  // killed before the request finishes. In this case, the caller should not
  // call FinishRequest.
  int StartRequest(ServiceWorkerMetrics::EventType event_type,
                   const StatusCallback& error_callback);

  // Same as StartRequest, but allows the caller to specify a custom timeout for
  // the event, as well as the behavior for when the request times out.
  int StartRequestWithCustomTimeout(ServiceWorkerMetrics::EventType event_type,
                                    const StatusCallback& error_callback,
                                    const base::TimeDelta& timeout,
                                    TimeoutBehavior timeout_behavior);

  // Starts a request of type EventType::EXTERNAL_REQUEST.
  // Provides a mechanism to external clients to keep the worker running.
  // |request_uuid| is a GUID for clients to identify the request.
  // Returns true if the request was successfully scheduled to starrt.
  bool StartExternalRequest(const std::string& request_uuid);

  // Informs ServiceWorkerVersion that an event has finished being dispatched.
  // Returns false if no pending requests with the provided id exist, for
  // example if the request has already timed out.
  // Pass the result of the event to |was_handled|, which is used to record
  // statistics based on the event status.
  // TODO(mek): Use something other than a bool for event status.
  bool FinishRequest(int request_id,
                     bool was_handled,
                     base::Time dispatch_event_time);

  // Finishes an external request that was started by StartExternalRequest().
  // Returns false if there was an error finishing the request: e.g. the request
  // was not found or the worker already terminated.
  bool FinishExternalRequest(const std::string& request_uuid);

  // This must be called when the worker is running.
  mojom::ServiceWorkerEventDispatcher* event_dispatcher() {
    DCHECK(event_dispatcher_.is_bound());
    return event_dispatcher_.get();
  }

  // Dispatches an event. If dispatching the event fails, all of the error
  // callbacks that were associated with |request_ids| via StartRequest are
  // called.
  // Use RegisterRequestCallback or RegisterSimpleRequest to register a callback
  // to receive messages sent back in response to this event before calling this
  // method.
  // This must be called when the worker is running.
  void DispatchEvent(const std::vector<int>& request_ids,
                     const IPC::Message& message);

  // This method registers a callback to receive messages sent back from the
  // service worker in response to |request_id|.
  // ResponseMessage is the type of the IPC message that is used for the
  // response, and its first argument MUST be the request_id.
  // Callback registration should be done once for one request_id.
  template <typename ResponseMessage, typename ResponseCallbackType>
  void RegisterRequestCallback(int request_id,
                               const ResponseCallbackType& callback);

  // You can use this method instead of RegisterRequestCallback when the
  // response message sent back from the service worker consists of just
  // a request_id and a blink::WebServiceWorkerEventResult field. The result
  // field is converted to a ServiceWorkerStatusCode and passed to the error
  // handler associated with the request_id which is registered by StartRequest.
  // Additionally if you use this method, FinishRequest will be called before
  // passing the reply to the callback.
  // Callback registration should be done once for one request_id.
  template <typename ResponseMessage>
  void RegisterSimpleRequest(int request_id);

  // This is a wrapper method equivalent to one RegisterSimpleRequest and one
  // DispatchEvent. For simple events where the full functionality of
  // RegisterRequestCallback/DispatchEvent is not needed, this method can be
  // used instead. The ResponseMessage must consist
  // of just a request_id and a blink::WebServiceWorkerEventResult field. The
  // result is converted to a ServiceWorkerStatusCode and passed to the error
  // handler associated with the request. Additionally this methods calls
  // FinishRequest before passing the reply to the callback.
  template <typename ResponseMessage>
  void DispatchSimpleEvent(int request_id, const IPC::Message& message);

  // Adds and removes |provider_host| as a controllee of this ServiceWorker.
  void AddControllee(ServiceWorkerProviderHost* provider_host);
  void RemoveControllee(ServiceWorkerProviderHost* provider_host);

  // Returns if it has controllee.
  bool HasControllee() const { return !controllee_map_.empty(); }
  std::map<std::string, ServiceWorkerProviderHost*> controllee_map() {
    return controllee_map_;
  }

  base::WeakPtr<ServiceWorkerContextCore> context() const { return context_; }

  // Adds and removes |request_job| as a dependent job not to stop the
  // ServiceWorker while |request_job| is reading the stream of the fetch event
  // response from the ServiceWorker.
  void AddStreamingURLRequestJob(const ServiceWorkerURLRequestJob* request_job);
  void RemoveStreamingURLRequestJob(
      const ServiceWorkerURLRequestJob* request_job);

  // Adds and removes Listeners.
  void AddListener(Listener* listener);
  void RemoveListener(Listener* listener);

  ServiceWorkerScriptCacheMap* script_cache_map() { return &script_cache_map_; }
  EmbeddedWorkerInstance* embedded_worker() { return embedded_worker_.get(); }

  // Reports the error message to |listeners_|.
  void ReportError(ServiceWorkerStatusCode status,
                   const std::string& status_message);

  // Sets the status code to pass to StartWorker callbacks if start fails.
  void SetStartWorkerStatusCode(ServiceWorkerStatusCode status);

  // Sets this version's status to REDUNDANT and deletes its resources.
  // The version must not have controllees.
  void Doom();
  bool is_redundant() const { return status_ == REDUNDANT; }

  bool skip_waiting() const { return skip_waiting_; }
  void set_skip_waiting(bool skip_waiting) { skip_waiting_ = skip_waiting; }

  bool skip_recording_startup_time() const {
    return skip_recording_startup_time_;
  }

  bool force_bypass_cache_for_scripts() const {
    return force_bypass_cache_for_scripts_;
  }
  void set_force_bypass_cache_for_scripts(bool force_bypass_cache_for_scripts) {
    force_bypass_cache_for_scripts_ = force_bypass_cache_for_scripts;
  }

  bool pause_after_download() const { return pause_after_download_; }
  void set_pause_after_download(bool pause_after_download) {
    pause_after_download_ = pause_after_download;
  }

  // Returns nullptr if the main script is not loaded yet and:
  //  1) The worker is a new one.
  //  OR
  //  2) The worker is an existing one but the entry in ServiceWorkerDatabase
  //     was written by old version of Chrome (< M56), so |origin_trial_tokens|
  //     wasn't set in the entry.
  const TrialTokenValidator::FeatureToTokensMap* origin_trial_tokens() const {
    return origin_trial_tokens_.get();
  }
  // Set valid tokens in |tokens|. Invalid tokens in |tokens| are ignored.
  void SetValidOriginTrialTokens(
      const TrialTokenValidator::FeatureToTokensMap& tokens);

  void SetDevToolsAttached(bool attached);

  // Sets the HttpResponseInfo used to load the main script.
  // This HttpResponseInfo will be used for all responses sent back from the
  // service worker, as the effective security of these responses is equivalent
  // to that of the ServiceWorker.
  void SetMainScriptHttpResponseInfo(const net::HttpResponseInfo& http_info);
  const net::HttpResponseInfo* GetMainScriptHttpResponseInfo();

  // Simulate ping timeout. Should be used for tests-only.
  void SimulatePingTimeoutForTesting();

  // Used to allow tests to change time for testing.
  void SetTickClockForTesting(std::unique_ptr<base::TickClock> tick_clock);

  // Returns true if the service worker has work to do: it has pending
  // requests, in-progress streaming URLRequestJobs, or pending start callbacks.
  bool HasWork() const;

  // Returns the number of pending external request count of this worker.
  size_t GetExternalRequestCountForTest() const {
    return external_request_uuid_to_request_id_.size();
  }

  // Returns the amount of time left until the request with the latest
  // expiration time expires.
  base::TimeDelta remaining_timeout() const {
    return max_request_expiration_time_ - tick_clock_->NowTicks();
  }

  // Callback function for simple events dispatched through mojo interface
  // mojom::ServiceWorkerEventDispatcher, once all simple events got dispatched
  // through mojo, OnSimpleEventResponse function could be removed.
  void OnSimpleEventFinished(int request_id,
                             ServiceWorkerStatusCode status,
                             base::Time dispatch_event_time);

  void NotifyMainScriptRequestHandlerCreated();
  void NotifyMainScriptJobCreated(
      ServiceWorkerContextRequestHandler::CreateJobStatus status);

 private:
  friend class base::RefCounted<ServiceWorkerVersion>;
  friend class ServiceWorkerMetrics;
  friend class ServiceWorkerReadFromCacheJobTest;
  friend class ServiceWorkerStallInStoppingTest;
  friend class ServiceWorkerURLRequestJobTest;
  friend class ServiceWorkerVersionBrowserTest;
  friend class ServiceWorkerVersionTestP;

  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerControlleeRequestHandlerTestP,
                           ActivateWaitingVersion);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerControlleeRequestHandlerTestP,
                           FallbackWithNoFetchHandler);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTestP, IdleTimeout);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTestP, SetDevToolsAttached);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTestP, StaleUpdate_FreshWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTestP,
                           StaleUpdate_NonActiveWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTestP, StaleUpdate_StartWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTestP,
                           StaleUpdate_RunningWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTestP,
                           StaleUpdate_DoNotDeferTimer);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTestP, RequestTimeout);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerFailToStartTest, Timeout);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionBrowserTest,
                           TimeoutStartingWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionBrowserTest,
                           TimeoutWorkerInEvent);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerStallInStoppingTest, DetachThenStart);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerStallInStoppingTest, DetachThenRestart);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTestP,
                           RegisterForeignFetchScopes);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTestP, RequestNowTimeout);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTestP, RequestNowTimeoutKill);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTestP, RequestCustomizedTimeout);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionTestP, MixedRequestTimeouts);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerURLRequestJobTest, EarlyResponse);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerURLRequestJobTest, CancelRequest);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerActivationTest, SkipWaiting);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerActivationTest,
                           SkipWaitingWithInflightRequest);

  class Metrics;
  class PingController;

  struct RequestInfo {
    RequestInfo(int id,
                ServiceWorkerMetrics::EventType event_type,
                const base::TimeTicks& expiration,
                TimeoutBehavior timeout_behavior);
    ~RequestInfo();
    bool operator>(const RequestInfo& other) const;
    int id;
    ServiceWorkerMetrics::EventType event_type;
    base::TimeTicks expiration;
    TimeoutBehavior timeout_behavior;
  };

  struct PendingRequest {
    PendingRequest(const StatusCallback& error_callback,
                   base::Time time,
                   const base::TimeTicks& time_ticks,
                   ServiceWorkerMetrics::EventType event_type);
    ~PendingRequest();

    // ------------------------------------------------------------------------
    // For all requests. Set by StartRequest.
    // ------------------------------------------------------------------------
    StatusCallback error_callback;
    base::Time start_time;
    base::TimeTicks start_time_ticks;
    ServiceWorkerMetrics::EventType event_type;

    // ------------------------------------------------------------------------
    // For IPC message requests.
    // ------------------------------------------------------------------------
    // Set by RegisterRequestCallback. Receives IPC responses to the request via
    // OnMessageReceived.
    std::unique_ptr<EmbeddedWorkerInstance::Listener> listener;
    // True if an IPC message was sent to dispatch the event for this request.
    bool is_dispatched = false;
  };


  typedef ServiceWorkerVersion self;
  using ServiceWorkerClients = std::vector<ServiceWorkerClientInfo>;
  using RequestInfoPriorityQueue =
      std::priority_queue<RequestInfo,
                          std::vector<RequestInfo>,
                          std::greater<RequestInfo>>;
  using WebStatusCallback =
      base::Callback<void(int, blink::WebServiceWorkerEventResult)>;

  // EmbeddedWorkerInstance Listener implementation which calls a callback
  // on receiving a particular IPC message. ResponseMessage is the type of
  // the IPC message to listen for, while CallbackType should be a callback
  // with same arguments as the IPC message.
  // Additionally only calls the callback for messages with a specific request
  // id, which must be the first argument of the IPC message.
  template <typename ResponseMessage, typename CallbackType>
  class EventResponseHandler : public EmbeddedWorkerInstance::Listener {
   public:
    EventResponseHandler(const base::WeakPtr<EmbeddedWorkerInstance>& worker,
                         int request_id,
                         const CallbackType& callback)
        : worker_(worker), request_id_(request_id), callback_(callback) {
      worker_->AddListener(this);
    }
    ~EventResponseHandler() override {
      if (worker_)
        worker_->RemoveListener(this);
    }
    bool OnMessageReceived(const IPC::Message& message) override;

   private:
    base::WeakPtr<EmbeddedWorkerInstance> const worker_;
    const int request_id_;
    const CallbackType callback_;
  };

  // The timeout timer interval.
  static const int kTimeoutTimerDelaySeconds;
  // Timeout for an installed worker to start.
  static const int kStartInstalledWorkerTimeoutSeconds;
  // Timeout for a new worker to start.
  static const int kStartNewWorkerTimeoutMinutes;
  // Timeout for a request to be handled.
  static const int kRequestTimeoutMinutes;
  // Timeout for the worker to stop.
  static const int kStopWorkerTimeoutSeconds;

  ~ServiceWorkerVersion() override;

  // The following methods all rely on the internal |tick_clock_| for the
  // current time.
  void RestartTick(base::TimeTicks* time) const;
  bool RequestExpired(const base::TimeTicks& expiration) const;
  base::TimeDelta GetTickDuration(const base::TimeTicks& time) const;

  // EmbeddedWorkerInstance::Listener overrides:
  void OnThreadStarted() override;
  void OnStarting() override;
  void OnStarted() override;
  void OnStopping() override;
  void OnStopped(EmbeddedWorkerStatus old_status) override;
  void OnDetached(EmbeddedWorkerStatus old_status) override;
  void OnScriptLoaded() override;
  void OnScriptLoadFailed() override;
  void OnRegisteredToDevToolsManager() override;
  void OnReportException(const base::string16& error_message,
                         int line_number,
                         int column_number,
                         const GURL& source_url) override;
  void OnReportConsoleMessage(int source_identifier,
                              int message_level,
                              const base::string16& message,
                              int line_number,
                              const GURL& source_url) override;
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnStartSentAndScriptEvaluated(ServiceWorkerStatusCode status);

  // Message handlers.

  // This corresponds to the spec's get(id) steps.
  void OnGetClient(int request_id, const std::string& client_uuid);

  // This corresponds to the spec's matchAll(options) steps.
  void OnGetClients(int request_id,
                    const ServiceWorkerClientQueryOptions& options);

  // Receiver function of responses of simple events dispatched through chromium
  // IPCs. This is internally the same with OnSimpleEventFinished and will be
  // replaced with OnSimpleEventFinished after all of simple events are
  // dispatched via mojo.
  void OnSimpleEventResponse(int request_id,
                             blink::WebServiceWorkerEventResult result,
                             base::Time dispatch_event_time);
  void OnOpenWindow(int request_id, GURL url);
  void OnOpenWindowFinished(int request_id,
                            ServiceWorkerStatusCode status,
                            const ServiceWorkerClientInfo& client_info);

  void OnSetCachedMetadata(const GURL& url, const std::vector<char>& data);
  void OnSetCachedMetadataFinished(int64_t callback_id, int result);
  void OnClearCachedMetadata(const GURL& url);
  void OnClearCachedMetadataFinished(int64_t callback_id, int result);

  void OnPostMessageToClient(const std::string& client_uuid,
                             const base::string16& message,
                             const std::vector<int>& sent_message_ports);
  void OnFocusClient(int request_id, const std::string& client_uuid);
  void OnNavigateClient(int request_id,
                        const std::string& client_uuid,
                        const GURL& url);
  void OnNavigateClientFinished(int request_id,
                                ServiceWorkerStatusCode status,
                                const ServiceWorkerClientInfo& client_info);
  void OnSkipWaiting(int request_id);
  void OnClaimClients(int request_id);
  void OnPongFromWorker();

  void OnFocusClientFinished(int request_id,
                             const ServiceWorkerClientInfo& client_info);

  void OnRegisterForeignFetchScopes(const std::vector<GURL>& sub_scopes,
                                    const std::vector<url::Origin>& origins);

  void DidEnsureLiveRegistrationForStartWorker(
      ServiceWorkerMetrics::EventType purpose,
      Status prestart_status,
      bool is_browser_startup_complete,
      const StatusCallback& callback,
      ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void StartWorkerInternal();

  void DidSkipWaiting(int request_id);

  void OnGetClientFinished(int request_id,
                           const ServiceWorkerClientInfo& client_info);

  void OnGetClientsFinished(int request_id, ServiceWorkerClients* clients);

  // The timeout timer periodically calls OnTimeoutTimer, which stops the worker
  // if it is excessively idle or unresponsive to ping.
  void StartTimeoutTimer();
  void StopTimeoutTimer();
  void OnTimeoutTimer();
  void SetTimeoutTimerInterval(base::TimeDelta interval);

  // Called by PingController for ping protocol.
  ServiceWorkerStatusCode PingWorker();
  void OnPingTimeout();

  // Stops the worker if it is idle (has no in-flight requests) or timed out
  // ping.
  void StopWorkerIfIdle();

  // RecordStartWorkerResult is added as a start callback by StartTimeoutTimer
  // and records metrics about startup.
  void RecordStartWorkerResult(ServiceWorkerMetrics::EventType purpose,
                               Status prestart_status,
                               int trace_id,
                               bool is_browser_startup_complete,
                               ServiceWorkerStatusCode status);

  bool MaybeTimeOutRequest(const RequestInfo& info);
  void SetAllRequestExpirations(const base::TimeTicks& expiration);

  // Returns the reason the embedded worker failed to start, using information
  // inaccessible to EmbeddedWorkerInstance. Returns |default_code| if it can't
  // deduce a reason.
  ServiceWorkerStatusCode DeduceStartWorkerFailureReason(
      ServiceWorkerStatusCode default_code);

  // Sets |stale_time_| if this worker is stale, causing an update to eventually
  // occur once the worker stops or is running too long.
  void MarkIfStale();

  void FoundRegistrationForUpdate(
      ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  void OnStoppedInternal(EmbeddedWorkerStatus old_status);

  // Called at the beginning of each Dispatch*Event function: records
  // the time elapsed since idle (generally the time since the previous
  // event ended).
  void OnBeginEvent();

  // Resets |start_worker_first_purpose_| and fires and clears all start
  // callbacks.
  void FinishStartWorker(ServiceWorkerStatusCode status);

  // Removes any pending external request that has GUID of |request_uuid|.
  void CleanUpExternalRequest(const std::string& request_uuid,
                              ServiceWorkerStatusCode status);

  const int64_t version_id_;
  const int64_t registration_id_;
  const GURL script_url_;
  const GURL scope_;
  std::vector<GURL> foreign_fetch_scopes_;
  std::vector<url::Origin> foreign_fetch_origins_;
  FetchHandlerExistence fetch_handler_existence_;
  // The source of truth for navigation preload state is the
  // ServiceWorkerRegistration. |navigation_preload_state_| is essentially a
  // cached value because it must be looked up quickly and a live registration
  // doesn't necessarily exist whenever there is a live version.
  NavigationPreloadState navigation_preload_state_;
  ServiceWorkerMetrics::Site site_for_uma_;

  Status status_ = NEW;
  std::unique_ptr<EmbeddedWorkerInstance> embedded_worker_;
  std::vector<StatusCallback> start_callbacks_;
  std::vector<StatusCallback> stop_callbacks_;
  std::vector<base::Closure> status_change_callbacks_;

  // Holds in-flight requests, including requests due to outstanding push,
  // fetch, sync, etc. events.
  IDMap<std::unique_ptr<PendingRequest>> pending_requests_;

  // Container for pending external requests for this service worker.
  // (key, value): (request uuid, request id).
  using RequestUUIDToRequestIDMap = std::map<std::string, int>;
  RequestUUIDToRequestIDMap external_request_uuid_to_request_id_;

  // Connected to ServiceWorkerContextClient while the worker is running.
  mojom::ServiceWorkerEventDispatcherPtr event_dispatcher_;

  std::set<const ServiceWorkerURLRequestJob*> streaming_url_request_jobs_;

  std::map<std::string, ServiceWorkerProviderHost*> controllee_map_;
  // Will be null while shutting down.
  base::WeakPtr<ServiceWorkerContextCore> context_;
  base::ObserverList<Listener> listeners_;
  ServiceWorkerScriptCacheMap script_cache_map_;
  base::OneShotTimer update_timer_;

  // Starts running in StartWorker and continues until the worker is stopped.
  base::RepeatingTimer timeout_timer_;
  // Holds the time the worker last started being considered idle.
  base::TimeTicks idle_time_;
  // Holds the time that the outstanding StartWorker() request started.
  base::TimeTicks start_time_;
  // Holds the time the worker entered STOPPING status. This is also used as a
  // trace event id.
  base::TimeTicks stop_time_;
  // Holds the time the worker was detected as stale and needs updating. We try
  // to update once the worker stops, but will also update if it stays alive too
  // long.
  base::TimeTicks stale_time_;
  // The latest expiration time of all requests that have ever been started. In
  // particular this is not just the maximum of the expiration times of all
  // currently existing requests, but also takes into account the former
  // expiration times of finished requests.
  base::TimeTicks max_request_expiration_time_;

  // Keeps track of requests for timeout purposes. Requests are sorted by
  // their expiration time (soonest to expire on top of the priority queue). The
  // timeout timer periodically checks |timeout_queue_| for entries that should
  // time out or have already been fulfilled (i.e., removed from
  // |pending_requests_|).
  RequestInfoPriorityQueue timeout_queue_;

  bool skip_waiting_ = false;
  bool skip_recording_startup_time_ = false;
  bool force_bypass_cache_for_scripts_ = false;
  bool pause_after_download_ = false;
  bool is_update_scheduled_ = false;
  bool in_dtor_ = false;

  std::vector<int> pending_skip_waiting_requests_;
  std::unique_ptr<net::HttpResponseInfo> main_script_http_info_;

  std::unique_ptr<TrialTokenValidator::FeatureToTokensMap> origin_trial_tokens_;

  // If not OK, the reason that StartWorker failed. Used for
  // running |start_callbacks_|.
  ServiceWorkerStatusCode start_worker_status_ = SERVICE_WORKER_OK;

  // The clock used to vend tick time.
  std::unique_ptr<base::TickClock> tick_clock_;

  std::unique_ptr<PingController> ping_controller_;
  std::unique_ptr<Metrics> metrics_;
  const bool should_exclude_from_uma_ = false;

  bool stop_when_devtools_detached_ = false;

  // Keeps the first purpose of starting the worker for UMA. Cleared in
  // FinishStartWorker().
  base::Optional<ServiceWorkerMetrics::EventType> start_worker_first_purpose_;

  bool main_script_request_handler_created_ = false;
  ServiceWorkerContextRequestHandler::CreateJobStatus main_script_job_created_ =
      ServiceWorkerContextRequestHandler::CreateJobStatus::UNINITIALIZED;

  base::WeakPtrFactory<ServiceWorkerVersion> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerVersion);
};

template <typename ResponseMessage>
void ServiceWorkerVersion::DispatchSimpleEvent(int request_id,
                                               const IPC::Message& message) {
  RegisterSimpleRequest<ResponseMessage>(request_id);
  DispatchEvent({request_id}, message);
}

template <typename ResponseMessage, typename ResponseCallbackType>
void ServiceWorkerVersion::RegisterRequestCallback(
    int request_id,
    const ResponseCallbackType& callback) {
  PendingRequest* request = pending_requests_.Lookup(request_id);
  DCHECK(request) << "Invalid request id";
  DCHECK(!request->listener) << "Callback was already registered";
  DCHECK(!request->is_dispatched) << "Request already dispatched an IPC event";
  request->listener.reset(
      new EventResponseHandler<ResponseMessage, ResponseCallbackType>(
          embedded_worker()->AsWeakPtr(), request_id, callback));
}

template <typename ResponseMessage>
void ServiceWorkerVersion::RegisterSimpleRequest(int request_id) {
  RegisterRequestCallback<ResponseMessage>(
      request_id,
      base::Bind(&ServiceWorkerVersion::OnSimpleEventResponse, this));
}

template <typename ResponseMessage, typename CallbackType>
bool ServiceWorkerVersion::EventResponseHandler<ResponseMessage, CallbackType>::
    OnMessageReceived(const IPC::Message& message) {
  if (message.type() != ResponseMessage::ID)
    return false;
  int received_request_id;
  bool result = base::PickleIterator(message).ReadInt(&received_request_id);
  if (!result || received_request_id != request_id_)
    return false;

  CallbackType protect(callback_);
  // Essentially same code as what IPC_MESSAGE_FORWARD expands to.
  void* param = nullptr;
  if (!ResponseMessage::Dispatch(&message, &callback_, this, param,
                                 &CallbackType::Run))
    message.set_dispatch_error();

  // At this point |this| can have been deleted, so don't do anything other
  // than returning.

  return true;
}

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_
