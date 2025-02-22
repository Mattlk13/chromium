// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_context.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/rand_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/push_messaging/push_messaging_router.h"
#include "content/browser/storage_partition_impl_map.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "net/cookies/cookie_store.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/channel_id_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/device/device_service.h"
#include "services/device/public/interfaces/constants.mojom.h"
#include "services/file/file_service.h"
#include "services/file/public/interfaces/constants.mojom.h"
#include "services/file/user_id_map.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/interfaces/service.mojom.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/fileapi/external_mount_points.h"

using base::UserDataAdapter;

namespace content {

namespace {

base::LazyInstance<std::map<std::string, BrowserContext*>>
    g_user_id_to_context = LAZY_INSTANCE_INITIALIZER;

class ServiceUserIdHolder : public base::SupportsUserData::Data {
 public:
  explicit ServiceUserIdHolder(const std::string& user_id)
      : user_id_(user_id) {}
  ~ServiceUserIdHolder() override {}

  const std::string& user_id() const { return user_id_; }

 private:
  std::string user_id_;

  DISALLOW_COPY_AND_ASSIGN(ServiceUserIdHolder);
};

// Key names on BrowserContext.
const char kDownloadManagerKeyName[] = "download_manager";
const char kMojoWasInitialized[] = "mojo-was-initialized";
const char kServiceManagerConnection[] = "service-manager-connection";
const char kServiceUserId[] = "service-user-id";
const char kStoragePartitionMapKeyName[] = "content_storage_partition_map";

#if defined(OS_CHROMEOS)
const char kMountPointsKey[] = "mount_points";
#endif  // defined(OS_CHROMEOS)

void RemoveBrowserContextFromUserIdMap(BrowserContext* browser_context) {
  ServiceUserIdHolder* holder = static_cast<ServiceUserIdHolder*>(
      browser_context->GetUserData(kServiceUserId));
  if (holder) {
    auto it = g_user_id_to_context.Get().find(holder->user_id());
    if (it != g_user_id_to_context.Get().end())
      g_user_id_to_context.Get().erase(it);
  }
}

StoragePartitionImplMap* GetStoragePartitionMap(
    BrowserContext* browser_context) {
  StoragePartitionImplMap* partition_map =
      static_cast<StoragePartitionImplMap*>(
          browser_context->GetUserData(kStoragePartitionMapKeyName));
  if (!partition_map) {
    partition_map = new StoragePartitionImplMap(browser_context);
    browser_context->SetUserData(kStoragePartitionMapKeyName, partition_map);
  }
  return partition_map;
}

StoragePartition* GetStoragePartitionFromConfig(
    BrowserContext* browser_context,
    const std::string& partition_domain,
    const std::string& partition_name,
    bool in_memory) {
  StoragePartitionImplMap* partition_map =
      GetStoragePartitionMap(browser_context);

  if (browser_context->IsOffTheRecord())
    in_memory = true;

  return partition_map->Get(partition_domain, partition_name, in_memory);
}

void SaveSessionStateOnIOThread(
    const scoped_refptr<net::URLRequestContextGetter>& context_getter,
    AppCacheServiceImpl* appcache_service) {
  net::URLRequestContext* context = context_getter->GetURLRequestContext();
  context->cookie_store()->SetForceKeepSessionState();
  context->channel_id_service()->GetChannelIDStore()->
      SetForceKeepSessionState();
  appcache_service->set_force_keep_session_state();
}

void SaveSessionStateOnIndexedDBThread(
    scoped_refptr<IndexedDBContextImpl> indexed_db_context) {
  indexed_db_context->SetForceKeepSessionState();
}

void ShutdownServiceWorkerContext(StoragePartition* partition) {
  ServiceWorkerContextWrapper* wrapper =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());
  wrapper->process_manager()->Shutdown();
}

void SetDownloadManager(BrowserContext* context,
                        content::DownloadManager* download_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(download_manager);
  context->SetUserData(kDownloadManagerKeyName, download_manager);
}

class BrowserContextServiceManagerConnectionHolder
    : public base::SupportsUserData::Data {
 public:
  BrowserContextServiceManagerConnectionHolder(
      std::unique_ptr<service_manager::Connection> connection,
      service_manager::mojom::ServiceRequest request)
      : root_connection_(std::move(connection)),
        service_manager_connection_(ServiceManagerConnection::Create(
            std::move(request),
            BrowserThread::GetTaskRunnerForThread(BrowserThread::IO))) {}
  ~BrowserContextServiceManagerConnectionHolder() override {}

  ServiceManagerConnection* service_manager_connection() {
    return service_manager_connection_.get();
  }

 private:
  std::unique_ptr<service_manager::Connection> root_connection_;
  std::unique_ptr<ServiceManagerConnection> service_manager_connection_;

  DISALLOW_COPY_AND_ASSIGN(BrowserContextServiceManagerConnectionHolder);
};

}  // namespace

// static
void BrowserContext::AsyncObliterateStoragePartition(
    BrowserContext* browser_context,
    const GURL& site,
    const base::Closure& on_gc_required) {
  GetStoragePartitionMap(browser_context)->AsyncObliterate(site,
                                                           on_gc_required);
}

// static
void BrowserContext::GarbageCollectStoragePartitions(
    BrowserContext* browser_context,
    std::unique_ptr<base::hash_set<base::FilePath>> active_paths,
    const base::Closure& done) {
  GetStoragePartitionMap(browser_context)
      ->GarbageCollect(std::move(active_paths), done);
}

DownloadManager* BrowserContext::GetDownloadManager(
    BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context->GetUserData(kDownloadManagerKeyName)) {
    DownloadManager* download_manager =
        new DownloadManagerImpl(
            GetContentClient()->browser()->GetNetLog(), context);

    SetDownloadManager(context, download_manager);
    download_manager->SetDelegate(context->GetDownloadManagerDelegate());
  }

  return static_cast<DownloadManager*>(
      context->GetUserData(kDownloadManagerKeyName));
}

// static
storage::ExternalMountPoints* BrowserContext::GetMountPoints(
    BrowserContext* context) {
  // Ensure that these methods are called on the UI thread, except for
  // unittests where a UI thread might not have been created.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsMessageLoopValid(BrowserThread::UI));

#if defined(OS_CHROMEOS)
  if (!context->GetUserData(kMountPointsKey)) {
    scoped_refptr<storage::ExternalMountPoints> mount_points =
        storage::ExternalMountPoints::CreateRefCounted();
    context->SetUserData(
        kMountPointsKey,
        new UserDataAdapter<storage::ExternalMountPoints>(mount_points.get()));
  }

  return UserDataAdapter<storage::ExternalMountPoints>::Get(context,
                                                            kMountPointsKey);
#else
  return NULL;
#endif
}

StoragePartition* BrowserContext::GetStoragePartition(
    BrowserContext* browser_context,
    SiteInstance* site_instance) {
  std::string partition_domain;
  std::string partition_name;
  bool in_memory = false;

  if (site_instance) {
    GetContentClient()->browser()->GetStoragePartitionConfigForSite(
        browser_context, site_instance->GetSiteURL(), true,
        &partition_domain, &partition_name, &in_memory);
  }

  return GetStoragePartitionFromConfig(
      browser_context, partition_domain, partition_name, in_memory);
}

StoragePartition* BrowserContext::GetStoragePartitionForSite(
    BrowserContext* browser_context,
    const GURL& site) {
  std::string partition_domain;
  std::string partition_name;
  bool in_memory;

  GetContentClient()->browser()->GetStoragePartitionConfigForSite(
      browser_context, site, true, &partition_domain, &partition_name,
      &in_memory);

  return GetStoragePartitionFromConfig(
      browser_context, partition_domain, partition_name, in_memory);
}

void BrowserContext::ForEachStoragePartition(
    BrowserContext* browser_context,
    const StoragePartitionCallback& callback) {
  StoragePartitionImplMap* partition_map =
      static_cast<StoragePartitionImplMap*>(
          browser_context->GetUserData(kStoragePartitionMapKeyName));
  if (!partition_map)
    return;

  partition_map->ForEach(callback);
}

StoragePartition* BrowserContext::GetDefaultStoragePartition(
    BrowserContext* browser_context) {
  return GetStoragePartition(browser_context, NULL);
}

// static
void BrowserContext::CreateMemoryBackedBlob(BrowserContext* browser_context,
                                            const char* data, size_t length,
                                            const BlobCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ChromeBlobStorageContext* blob_context =
      ChromeBlobStorageContext::GetFor(browser_context);
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ChromeBlobStorageContext::CreateMemoryBackedBlob,
                 make_scoped_refptr(blob_context), data, length),
      callback);
}

// static
void BrowserContext::CreateFileBackedBlob(
    BrowserContext* browser_context,
    const base::FilePath& path,
    int64_t offset,
    int64_t size,
    const base::Time& expected_modification_time,
    const BlobCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ChromeBlobStorageContext* blob_context =
      ChromeBlobStorageContext::GetFor(browser_context);
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ChromeBlobStorageContext::CreateFileBackedBlob,
                 make_scoped_refptr(blob_context), path, offset, size,
                 expected_modification_time),
      callback);
}

// static
void BrowserContext::DeliverPushMessage(
    BrowserContext* browser_context,
    const GURL& origin,
    int64_t service_worker_registration_id,
    const PushEventPayload& payload,
    const base::Callback<void(PushDeliveryStatus)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingRouter::DeliverMessage(browser_context, origin,
                                      service_worker_registration_id, payload,
                                      callback);
}

// static
void BrowserContext::NotifyWillBeDestroyed(BrowserContext* browser_context) {
  // Service Workers must shutdown before the browser context is destroyed,
  // since they keep render process hosts alive and the codebase assumes that
  // render process hosts die before their profile (browser context) dies.
  ForEachStoragePartition(browser_context,
                          base::Bind(ShutdownServiceWorkerContext));

  // Shared workers also keep render process hosts alive, and are expected to
  // return ref counts to 0 after documents close. However, shared worker
  // bookkeeping is done on the IO thread and we want to ensure the hosts are
  // destructed now, so forcibly release their ref counts here.
  for (RenderProcessHost::iterator host_iterator =
           RenderProcessHost::AllHostsIterator();
       !host_iterator.IsAtEnd(); host_iterator.Advance()) {
    RenderProcessHost* host = host_iterator.GetCurrentValue();
    if (host->GetBrowserContext() == browser_context)
      host->ForceReleaseWorkerRefCounts();
  }
}

void BrowserContext::EnsureResourceContextInitialized(BrowserContext* context) {
  // This will be enough to tickle initialization of BrowserContext if
  // necessary, which initializes ResourceContext. The reason we don't call
  // ResourceContext::InitializeResourceContext() directly here is that
  // ResourceContext initialization may call back into BrowserContext
  // and when that call returns it'll end rewriting its UserData map. It will
  // end up rewriting the same value but this still causes a race condition.
  //
  // See http://crbug.com/115678.
  GetDefaultStoragePartition(context);
}

void BrowserContext::SaveSessionState(BrowserContext* browser_context) {
  GetDefaultStoragePartition(browser_context)->GetDatabaseTracker()->
      SetForceKeepSessionState();
  StoragePartition* storage_partition =
      BrowserContext::GetDefaultStoragePartition(browser_context);

  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &SaveSessionStateOnIOThread,
            make_scoped_refptr(BrowserContext::GetDefaultStoragePartition(
                browser_context)->GetURLRequestContext()),
            static_cast<AppCacheServiceImpl*>(
                storage_partition->GetAppCacheService())));
  }

  DOMStorageContextWrapper* dom_storage_context_proxy =
      static_cast<DOMStorageContextWrapper*>(
          storage_partition->GetDOMStorageContext());
  dom_storage_context_proxy->SetForceKeepSessionState();

  IndexedDBContextImpl* indexed_db_context_impl =
      static_cast<IndexedDBContextImpl*>(
        storage_partition->GetIndexedDBContext());
  // No task runner in unit tests.
  if (indexed_db_context_impl->TaskRunner()) {
    indexed_db_context_impl->TaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&SaveSessionStateOnIndexedDBThread,
                   make_scoped_refptr(indexed_db_context_impl)));
  }
}

void BrowserContext::SetDownloadManagerForTesting(
    BrowserContext* browser_context,
    DownloadManager* download_manager) {
  SetDownloadManager(browser_context, download_manager);
}

// static
void BrowserContext::Initialize(
    BrowserContext* browser_context,
    const base::FilePath& path) {

  std::string new_id;
  if (GetContentClient() && GetContentClient()->browser()) {
    new_id = GetContentClient()->browser()->GetServiceUserIdForBrowserContext(
        browser_context);
  } else {
    // Some test scenarios initialize a BrowserContext without a content client.
    new_id = base::GenerateGUID();
  }

  ServiceUserIdHolder* holder = static_cast<ServiceUserIdHolder*>(
      browser_context->GetUserData(kServiceUserId));
  if (holder)
    file::ForgetServiceUserIdUserDirAssociation(holder->user_id());
  file::AssociateServiceUserIdWithUserDir(new_id, path);
  RemoveBrowserContextFromUserIdMap(browser_context);
  g_user_id_to_context.Get()[new_id] = browser_context;
  browser_context->SetUserData(kServiceUserId,
                               new ServiceUserIdHolder(new_id));

  browser_context->SetUserData(kMojoWasInitialized,
                               new base::SupportsUserData::Data);

  ServiceManagerConnection* service_manager_connection =
      ServiceManagerConnection::GetForProcess();
  if (service_manager_connection && base::ThreadTaskRunnerHandle::IsSet()) {
    // NOTE: Many unit tests create a TestBrowserContext without initializing
    // Mojo or the global service manager connection.

    service_manager::mojom::ServicePtr service;
    service_manager::mojom::ServiceRequest service_request(&service);

    service_manager::mojom::PIDReceiverPtr pid_receiver;
    service_manager::Identity identity(mojom::kBrowserServiceName, new_id);
    service_manager_connection->GetConnector()->StartService(
        identity, std::move(service), mojo::MakeRequest(&pid_receiver));
    pid_receiver->SetPID(base::GetCurrentProcId());

    BrowserContextServiceManagerConnectionHolder* connection_holder =
        new BrowserContextServiceManagerConnectionHolder(
            service_manager_connection->GetConnector()->Connect(identity),
            std::move(service_request));
    browser_context->SetUserData(kServiceManagerConnection, connection_holder);

    ServiceManagerConnection* connection =
        connection_holder->service_manager_connection();
    connection->Start();

    // New embedded service factories should be added to |connection| here.
    // TODO(blundell): Does this belong as a global service rather than per
    // BrowserContext?
    ServiceInfo info;
    info.factory =
        base::Bind(&device::CreateDeviceService,
                   BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE));
    connection->AddEmbeddedService(device::mojom::kServiceName, info);

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kMojoLocalStorage)) {
      ServiceInfo info;
      info.factory =
          base::Bind(&file::CreateFileService,
                     BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE),
                     BrowserThread::GetTaskRunnerForThread(BrowserThread::DB));
      connection->AddEmbeddedService(file::mojom::kServiceName, info);
    }
  }
}

// static
const std::string& BrowserContext::GetServiceUserIdFor(
    BrowserContext* browser_context) {
  CHECK(browser_context->GetUserData(kMojoWasInitialized))
      << "Attempting to get the mojo user id for a BrowserContext that was "
      << "never Initialize()ed.";

  ServiceUserIdHolder* holder = static_cast<ServiceUserIdHolder*>(
      browser_context->GetUserData(kServiceUserId));
  return holder->user_id();
}

// static
BrowserContext* BrowserContext::GetBrowserContextForServiceUserId(
    const std::string& user_id) {
  auto it = g_user_id_to_context.Get().find(user_id);
  return it != g_user_id_to_context.Get().end() ? it->second : nullptr;
}

// static
service_manager::Connector* BrowserContext::GetConnectorFor(
    BrowserContext* browser_context) {
  ServiceManagerConnection* connection =
      GetServiceManagerConnectionFor(browser_context);
  return connection ? connection->GetConnector() : nullptr;
}

// static
ServiceManagerConnection* BrowserContext::GetServiceManagerConnectionFor(
    BrowserContext* browser_context) {
  BrowserContextServiceManagerConnectionHolder* connection_holder =
      static_cast<BrowserContextServiceManagerConnectionHolder*>(
          browser_context->GetUserData(kServiceManagerConnection));
  return connection_holder ? connection_holder->service_manager_connection()
                           : nullptr;
}

BrowserContext::~BrowserContext() {
  CHECK(GetUserData(kMojoWasInitialized))
      << "Attempting to destroy a BrowserContext that never called "
      << "Initialize()";

  DCHECK(!GetUserData(kStoragePartitionMapKeyName))
      << "StoragePartitionMap is not shut down properly";

  RemoveBrowserContextFromUserIdMap(this);

  if (GetUserData(kDownloadManagerKeyName))
    GetDownloadManager(this)->Shutdown();
}

void BrowserContext::ShutdownStoragePartitions() {
  if (GetUserData(kStoragePartitionMapKeyName))
    RemoveUserData(kStoragePartitionMapKeyName);
}

}  // namespace content
