// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/metrics/ios_chrome_metrics_service_client.h"

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/strings/string16.h"
#include "base/threading/platform_thread.h"
#include "components/crash/core/common/crash_keys.h"
#include "components/metrics/call_stack_profile_metrics_provider.h"
#include "components/metrics/drive_metrics_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/net/cellular_logic_helper.h"
#include "components/metrics/net/net_metrics_log_uploader.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "components/metrics/net/version_utils.h"
#include "components/metrics/profiler/profiler_metrics_provider.h"
#include "components/metrics/profiler/tracking_synchronizer.h"
#include "components/metrics/stability_metrics_helper.h"
#include "components/metrics/ui/screen_info_metrics_provider.h"
#include "components/metrics/url_constants.h"
#include "components/omnibox/browser/omnibox_metrics_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/signin_status_metrics_provider.h"
#include "components/sync/device_info/device_count_metrics_provider.h"
#include "components/translate/core/browser/translate_ranker_metrics_provider.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/version_info.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/google/google_brand.h"
#include "ios/chrome/browser/metrics/ios_chrome_stability_metrics_provider.h"
#include "ios/chrome/browser/metrics/mobile_session_shutdown_metrics_provider.h"
#include "ios/chrome/browser/signin/ios_chrome_signin_status_metrics_provider_delegate.h"
#include "ios/chrome/browser/sync/ios_chrome_sync_client.h"
#include "ios/chrome/browser/tab_parenting_global_observer.h"
#include "ios/chrome/browser/ui/browser_list_ios.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/web_thread.h"

IOSChromeMetricsServiceClient::IOSChromeMetricsServiceClient(
    metrics::MetricsStateManager* state_manager)
    : metrics_state_manager_(state_manager),
      stability_metrics_provider_(nullptr),
      profiler_metrics_provider_(nullptr),
      drive_metrics_provider_(nullptr),
      start_time_(base::TimeTicks::Now()),
      has_uploaded_profiler_data_(false),
      weak_ptr_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  RegisterForNotifications();
}

IOSChromeMetricsServiceClient::~IOSChromeMetricsServiceClient() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

// static
std::unique_ptr<IOSChromeMetricsServiceClient>
IOSChromeMetricsServiceClient::Create(
    metrics::MetricsStateManager* state_manager) {
  // Perform two-phase initialization so that |client->metrics_service_| only
  // receives pointers to fully constructed objects.
  std::unique_ptr<IOSChromeMetricsServiceClient> client(
      new IOSChromeMetricsServiceClient(state_manager));
  client->Initialize();

  return client;
}

// static
void IOSChromeMetricsServiceClient::RegisterPrefs(
    PrefRegistrySimple* registry) {
  metrics::MetricsService::RegisterPrefs(registry);
  metrics::StabilityMetricsHelper::RegisterPrefs(registry);
  metrics::RegisterMetricsReportingStatePrefs(registry);
}

metrics::MetricsService* IOSChromeMetricsServiceClient::GetMetricsService() {
  return metrics_service_.get();
}

void IOSChromeMetricsServiceClient::SetMetricsClientId(
    const std::string& client_id) {
  crash_keys::SetMetricsClientIdFromGUID(client_id);
}

int32_t IOSChromeMetricsServiceClient::GetProduct() {
  return metrics::ChromeUserMetricsExtension::CHROME;
}

std::string IOSChromeMetricsServiceClient::GetApplicationLocale() {
  return GetApplicationContext()->GetApplicationLocale();
}

bool IOSChromeMetricsServiceClient::GetBrand(std::string* brand_code) {
  return ios::google_brand::GetBrand(brand_code);
}

metrics::SystemProfileProto::Channel
IOSChromeMetricsServiceClient::GetChannel() {
  return metrics::AsProtobufChannel(::GetChannel());
}

std::string IOSChromeMetricsServiceClient::GetVersionString() {
  return metrics::GetVersionString();
}

void IOSChromeMetricsServiceClient::OnLogUploadComplete() {}

void IOSChromeMetricsServiceClient::InitializeSystemProfileMetrics(
    const base::Closure& done_callback) {
  finished_init_task_callback_ = done_callback;
  drive_metrics_provider_->GetDriveMetrics(
      base::Bind(&IOSChromeMetricsServiceClient::OnInitTaskGotDriveMetrics,
                 weak_ptr_factory_.GetWeakPtr()));
}

void IOSChromeMetricsServiceClient::CollectFinalMetricsForLog(
    const base::Closure& done_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  collect_final_metrics_done_callback_ = done_callback;

  if (ShouldIncludeProfilerDataInLog()) {
    // Fetch profiler data. This will call into
    // |FinishedReceivingProfilerData()| when the task completes.
    metrics::TrackingSynchronizer::FetchProfilerDataAsynchronously(
        weak_ptr_factory_.GetWeakPtr());
  } else {
    CollectFinalHistograms();
  }
}

std::unique_ptr<metrics::MetricsLogUploader>
IOSChromeMetricsServiceClient::CreateUploader(
    const std::string& server_url,
    const std::string& mime_type,
    const base::Callback<void(int)>& on_upload_complete) {
  return base::MakeUnique<metrics::NetMetricsLogUploader>(
      GetApplicationContext()->GetSystemURLRequestContext(),
      server_url, mime_type, on_upload_complete);
}

base::TimeDelta IOSChromeMetricsServiceClient::GetStandardUploadInterval() {
  return metrics::GetUploadInterval();
}

base::string16 IOSChromeMetricsServiceClient::GetRegistryBackupKey() {
  return base::string16();
}

void IOSChromeMetricsServiceClient::OnRendererProcessCrash() {
  stability_metrics_provider_->LogRendererCrash();
}

void IOSChromeMetricsServiceClient::WebStateDidStartLoading(
    web::WebState* web_state) {
  metrics_service_->OnApplicationNotIdle();
}

void IOSChromeMetricsServiceClient::WebStateDidStopLoading(
    web::WebState* web_state) {
  metrics_service_->OnApplicationNotIdle();
}

void IOSChromeMetricsServiceClient::Initialize() {
  metrics_service_ = base::MakeUnique<metrics::MetricsService>(
      metrics_state_manager_, this, GetApplicationContext()->GetLocalState());

  // Register metrics providers.
  metrics_service_->RegisterMetricsProvider(
      base::MakeUnique<metrics::NetworkMetricsProvider>(
          web::WebThread::GetBlockingPool()));

  // Currently, we configure OmniboxMetricsProvider to not log events to UMA
  // if there is a single incognito session visible. In the future, it may
  // be worth revisiting this to still log events from non-incognito sessions.
  metrics_service_->RegisterMetricsProvider(
      base::MakeUnique<OmniboxMetricsProvider>(
          base::Bind(&BrowserListIOS::IsOffTheRecordSessionActive)));

  {
    auto stability_metrics_provider =
        base::MakeUnique<IOSChromeStabilityMetricsProvider>(
            GetApplicationContext()->GetLocalState());
    stability_metrics_provider_ = stability_metrics_provider.get();
    metrics_service_->RegisterMetricsProvider(
        std::move(stability_metrics_provider));
  }

  metrics_service_->RegisterMetricsProvider(
      base::MakeUnique<metrics::ScreenInfoMetricsProvider>());

  {
    auto drive_metrics_provider =
        base::MakeUnique<metrics::DriveMetricsProvider>(
            web::WebThread::GetTaskRunnerForThread(web::WebThread::FILE),
            ios::FILE_LOCAL_STATE);
    drive_metrics_provider_ = drive_metrics_provider.get();
    metrics_service_->RegisterMetricsProvider(
        std::move(drive_metrics_provider));
  }

  {
    auto profiler_metrics_provider =
        base::MakeUnique<metrics::ProfilerMetricsProvider>(
            base::Bind(&metrics::IsCellularLogicEnabled));
    profiler_metrics_provider_ = profiler_metrics_provider.get();
    metrics_service_->RegisterMetricsProvider(
        std::move(profiler_metrics_provider));
  }

  metrics_service_->RegisterMetricsProvider(
      base::MakeUnique<metrics::CallStackProfileMetricsProvider>());

  metrics_service_->RegisterMetricsProvider(
      SigninStatusMetricsProvider::CreateInstance(
          base::MakeUnique<IOSChromeSigninStatusMetricsProviderDelegate>()));

  metrics_service_->RegisterMetricsProvider(
      base::MakeUnique<MobileSessionShutdownMetricsProvider>(
          metrics_service_.get()));

  metrics_service_->RegisterMetricsProvider(
      base::MakeUnique<syncer::DeviceCountMetricsProvider>(
          base::Bind(&IOSChromeSyncClient::GetDeviceInfoTrackers)));

  metrics_service_->RegisterMetricsProvider(
      base::MakeUnique<translate::TranslateRankerMetricsProvider>());
}

void IOSChromeMetricsServiceClient::OnInitTaskGotDriveMetrics() {
  finished_init_task_callback_.Run();
}

bool IOSChromeMetricsServiceClient::ShouldIncludeProfilerDataInLog() {
  // Upload profiler data at most once per session.
  if (has_uploaded_profiler_data_)
    return false;

  // For each log, flip a fair coin. Thus, profiler data is sent with the first
  // log with probability 50%, with the second log with probability 25%, and so
  // on. As a result, uploaded data is biased toward earlier logs.
  // TODO(isherman): Explore other possible algorithms, and choose one that
  // might be more appropriate.  For example, it might be reasonable to include
  // profiler data with some fixed probability, so that a given client might
  // upload profiler data more than once; but on average, clients won't upload
  // too much data.
  if (base::RandDouble() < 0.5)
    return false;

  has_uploaded_profiler_data_ = true;
  return true;
}

void IOSChromeMetricsServiceClient::ReceivedProfilerData(
    const metrics::ProfilerDataAttributes& attributes,
    const tracked_objects::ProcessDataPhaseSnapshot& process_data_phase,
    const metrics::ProfilerEvents& past_events) {
  profiler_metrics_provider_->RecordProfilerData(
      process_data_phase, attributes.process_id, attributes.process_type,
      attributes.profiling_phase, attributes.phase_start - start_time_,
      attributes.phase_finish - start_time_, past_events);
}

void IOSChromeMetricsServiceClient::FinishedReceivingProfilerData() {
  CollectFinalHistograms();
}

void IOSChromeMetricsServiceClient::CollectFinalHistograms() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(ios): Try to extract the flow below into a utility function that is
  // shared between the iOS port's usage and
  // ChromeMetricsServiceClient::CollectFinalHistograms()'s usage of
  // MetricsMemoryDetails.
  std::unique_ptr<base::ProcessMetrics> process_metrics(
      base::ProcessMetrics::CreateProcessMetrics(
          base::GetCurrentProcessHandle()));
  UMA_HISTOGRAM_MEMORY_KB("Memory.Browser",
                          process_metrics->GetWorkingSetSize() / 1024);
  collect_final_metrics_done_callback_.Run();
}

void IOSChromeMetricsServiceClient::RegisterForNotifications() {
  tab_parented_subscription_ =
      TabParentingGlobalObserver::GetInstance()->RegisterCallback(
          base::Bind(&IOSChromeMetricsServiceClient::OnTabParented,
                     base::Unretained(this)));
  omnibox_url_opened_subscription_ =
      OmniboxEventGlobalTracker::GetInstance()->RegisterCallback(
          base::Bind(&IOSChromeMetricsServiceClient::OnURLOpenedFromOmnibox,
                     base::Unretained(this)));
}

void IOSChromeMetricsServiceClient::OnTabParented(web::WebState* web_state) {
  metrics_service_->OnApplicationNotIdle();
}

void IOSChromeMetricsServiceClient::OnURLOpenedFromOmnibox(OmniboxLog* log) {
  metrics_service_->OnApplicationNotIdle();
}

metrics::EnableMetricsDefault
IOSChromeMetricsServiceClient::GetMetricsReportingDefaultState() {
  return metrics::GetMetricsReportingDefaultState(
      GetApplicationContext()->GetLocalState());
}
