// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_HOST_STUB_H_
#define COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_HOST_STUB_H_

#include <string>

#include "components/sync/engine/sync_engine_host.h"

namespace syncer {

class SyncEngineHostStub : public SyncEngineHost {
 public:
  SyncEngineHostStub();
  ~SyncEngineHostStub() override;

  // SyncEngineHost implementation.
  void OnEngineInitialized(
      ModelTypeSet initial_types,
      const WeakHandle<JsBackend>& js_backend,
      const WeakHandle<DataTypeDebugInfoListener>& debug_info_listener,
      const std::string& cache_guid,
      bool success) override;
  void OnSyncCycleCompleted() override;
  void OnProtocolEvent(const ProtocolEvent& event) override;
  void OnDirectoryTypeCommitCounterUpdated(
      ModelType type,
      const CommitCounters& counters) override;
  void OnDirectoryTypeUpdateCounterUpdated(
      ModelType type,
      const UpdateCounters& counters) override;
  void OnDatatypeStatusCounterUpdated(ModelType type,
                                      const StatusCounters& counters) override;
  void OnConnectionStatusChange(ConnectionStatus status) override;
  void OnPassphraseRequired(
      PassphraseRequiredReason reason,
      const sync_pb::EncryptedData& pending_keys) override;
  void OnPassphraseAccepted() override;
  void OnEncryptedTypesChanged(ModelTypeSet encrypted_types,
                               bool encrypt_everything) override;
  void OnEncryptionComplete() override;
  void OnMigrationNeededForTypes(ModelTypeSet types) override;
  void OnExperimentsChanged(const Experiments& experiments) override;
  void OnActionableError(const SyncProtocolError& error) override;
  void OnLocalSetPassphraseEncryption(
      const SyncEncryptionHandler::NigoriState& nigori_state) override;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_SYNC_ENGINE_HOST_STUB_H_
