// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_MODEL_TYPE_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/driver/data_type_controller.h"

namespace syncer {

class SyncClient;
struct ActivationContext;

// DataTypeController implementation for Unified Sync and Storage model types.
class ModelTypeController : public DataTypeController {
 public:
  // |dump_stack| is called when an unrecoverable error occurs.
  ModelTypeController(
      ModelType type,
      const base::Closure& dump_stack,
      SyncClient* sync_client,
      const scoped_refptr<base::SingleThreadTaskRunner>& model_thread);
  ~ModelTypeController() override;

  // DataTypeController implementation.
  bool ShouldLoadModelBeforeConfigure() const override;
  void LoadModels(const ModelLoadCallback& model_load_callback) override;
  void GetAllNodes(const AllNodesCallback& callback) override;
  void GetStatusCounters(const StatusCountersCallback& callback) override;
  void RegisterWithBackend(base::Callback<void(bool)> set_downloaded,
                           ModelTypeConfigurer* configurer) override;
  void StartAssociating(const StartCallback& start_callback) override;
  void ActivateDataType(ModelTypeConfigurer* configurer) override;
  void DeactivateDataType(ModelTypeConfigurer* configurer) override;
  void Stop() override;
  std::string name() const override;
  State state() const override;

 protected:
  std::unique_ptr<DataTypeErrorHandler> CreateErrorHandler() override;

 private:
  void RecordStartFailure(ConfigureResult result) const;
  void ReportLoadModelError(const SyncError& error);

  // If the DataType controller is waiting for models to load, once the models
  // are loaded this function should be called to let the base class
  // implementation know that it is safe to continue with the activation.
  // The error indicates whether the loading completed successfully.
  void LoadModelsDone(ConfigureResult result, const SyncError& error);

  // The function will do the real work when OnProcessorStarted got called. This
  // is called on the UI thread.
  void OnProcessorStarted(
      SyncError error,
      std::unique_ptr<ActivationContext> activation_context);

  // The sync client, which provides access to this type's ModelTypeSyncBridge.
  SyncClient* const sync_client_;

  // The thread the model type lives on.
  scoped_refptr<base::SingleThreadTaskRunner> model_thread_;

  // Sync prefs. Used for determinig if DisableSync should be called during call
  // to Stop().
  SyncPrefs sync_prefs_;

  // State of this datatype controller.
  State state_;

  // Callbacks for use when starting the datatype.
  ModelLoadCallback model_load_callback_;

  // Controller receives |activation_context_| from SharedModelTypeProcessor
  // callback and must temporarily own it until ActivateDataType is called.
  std::unique_ptr<ActivationContext> activation_context_;

  // This is a hack to prevent reconfigurations from crashing, because USS
  // activation is not idempotent. RegisterWithBackend only needs to actually do
  // something the first time after the type is enabled.
  // TODO(crbug.com/647505): Remove this once the DTM handles things better.
  bool activated_ = false;

  DISALLOW_COPY_AND_ASSIGN(ModelTypeController);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_MODEL_TYPE_CONTROLLER_H_
