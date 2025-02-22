// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_LOCAL_STORAGE_CONTEXT_MOJO_H_
#define CONTENT_BROWSER_DOM_STORAGE_LOCAL_STORAGE_CONTEXT_MOJO_H_

#include <memory>

#include "base/files/file_path.h"
#include "content/common/content_export.h"
#include "content/common/leveldb_wrapper.mojom.h"
#include "services/file/public/interfaces/file_system.mojom.h"
#include "url/origin.h"

namespace service_manager {
class Connection;
class Connector;
}

namespace content {

class LevelDBWrapperImpl;
struct LocalStorageUsageInfo;

// Used for mojo-based LocalStorage implementation (behind --mojo-local-storage
// for now).
class CONTENT_EXPORT LocalStorageContextMojo {
 public:
  using GetStorageUsageCallback =
      base::OnceCallback<void(std::vector<LocalStorageUsageInfo>)>;

  LocalStorageContextMojo(service_manager::Connector* connector,
                          const base::FilePath& subdirectory);
  ~LocalStorageContextMojo();

  void OpenLocalStorage(const url::Origin& origin,
                        mojom::LevelDBWrapperRequest request);
  void GetStorageUsage(GetStorageUsageCallback callback);
  void DeleteStorage(const url::Origin& origin);
  // Like DeleteStorage(), but also deletes storage for all sub-origins.
  void DeleteStorageForPhysicalOrigin(const url::Origin& origin);
  void Flush();

  void SetDatabaseForTesting(leveldb::mojom::LevelDBDatabasePtr database);

 private:
  // Runs |callback| immediately if already connected to a database, otherwise
  // delays running |callback| untill after a connection has been established.
  // Initiates connecting to the database if no connection is in progres yet.
  void RunWhenConnected(base::OnceClosure callback);

  void OnLevelDBWrapperHasNoBindings(const url::Origin& origin);
  std::vector<leveldb::mojom::BatchedOperationPtr>
  OnLevelDBWrapperPrepareToCommit(const url::Origin& origin,
                                  const LevelDBWrapperImpl& wrapper);
  void OnUserServiceConnectionComplete();
  void OnUserServiceConnectionError();

  // Part of our asynchronous directory opening called from OpenLocalStorage().
  void OnDirectoryOpened(filesystem::mojom::FileError err);
  void OnDatabaseOpened(leveldb::mojom::DatabaseError status);
  void OnGotDatabaseVersion(leveldb::mojom::DatabaseError status,
                            const std::vector<uint8_t>& value);

  // The (possibly delayed) implementation of OpenLocalStorage(). Can be called
  // directly from that function, or through |on_database_open_callbacks_|.
  void BindLocalStorage(const url::Origin& origin,
                        mojom::LevelDBWrapperRequest request);
  LevelDBWrapperImpl* GetOrCreateDBWrapper(const url::Origin& origin);

  // The (possibly delayed) implementation of GetStorageUsage(). Can be called
  // directly from that function, or through |on_database_open_callbacks_|.
  void RetrieveStorageUsage(GetStorageUsageCallback callback);
  void OnGotMetaData(GetStorageUsageCallback callback,
                     leveldb::mojom::DatabaseError status,
                     std::vector<leveldb::mojom::KeyValuePtr> data);

  void OnGotStorageUsageForDeletePhysicalOrigin(
      const url::Origin& origin,
      std::vector<LocalStorageUsageInfo> usage);

  service_manager::Connector* const connector_;
  const base::FilePath subdirectory_;

  enum ConnectionState {
    NO_CONNECTION,
    CONNECTION_IN_PROGRESS,
    CONNECTION_FINISHED
  } connection_state_ = NO_CONNECTION;
  bool database_initialized_ = false;

  std::unique_ptr<service_manager::Connection> file_service_connection_;

  file::mojom::FileSystemPtr file_system_;
  filesystem::mojom::DirectoryPtr directory_;

  leveldb::mojom::LevelDBServicePtr leveldb_service_;
  leveldb::mojom::LevelDBDatabasePtr database_;

  std::vector<base::OnceClosure> on_database_opened_callbacks_;

  // Maps between an origin and its prefixed LevelDB view.
  std::map<url::Origin, std::unique_ptr<LevelDBWrapperImpl>> level_db_wrappers_;

  base::WeakPtrFactory<LocalStorageContextMojo> weak_ptr_factory_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_LOCAL_STORAGE_CONTEXT_MOJO_H_
