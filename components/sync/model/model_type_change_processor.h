// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MODEL_TYPE_CHANGE_PROCESSOR_H_
#define COMPONENTS_SYNC_MODEL_MODEL_TYPE_CHANGE_PROCESSOR_H_

#include <memory>
#include <string>

#include "base/location.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/activation_context.h"
#include "components/sync/model/data_type_error_handler.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/model_error.h"

namespace syncer {

class MetadataBatch;
class MetadataChangeList;
class ModelTypeSyncBridge;
class SyncError;

// Interface used by the ModelTypeSyncBridge to inform sync of local
// changes.
class ModelTypeChangeProcessor {
 public:
  typedef base::Callback<void(SyncError, std::unique_ptr<ActivationContext>)>
      StartCallback;

  // A factory function to make an implementation of ModelTypeChangeProcessor.
  static std::unique_ptr<ModelTypeChangeProcessor> Create(
      ModelType type,
      ModelTypeSyncBridge* bridge);

  ModelTypeChangeProcessor();
  virtual ~ModelTypeChangeProcessor();

  // Inform the processor of a new or updated entity. The |entity_data| param
  // does not need to be fully set, but it should at least have specifics and
  // non-unique name. The processor will fill in the rest if the bridge does
  // not have a reason to care.
  virtual void Put(const std::string& storage_key,
                   std::unique_ptr<EntityData> entity_data,
                   MetadataChangeList* metadata_change_list) = 0;

  // Inform the processor of a deleted entity.
  virtual void Delete(const std::string& storage_key,
                      MetadataChangeList* metadata_change_list) = 0;

  // Accept the initial sync metadata loaded by the bridge. This must be called
  // by the bridge for syncing to begin for this model type. If an error occurs,
  // call ReportError instead of this.
  virtual void OnMetadataLoaded(std::unique_ptr<MetadataBatch> batch) = 0;

  // Indicates that sync wants to connect a sync worker to this processor. Once
  // the processor has metadata from the bridge, it will pass the info needed
  // for the worker into |callback|. |error_handler| is how the processor will
  // inform sync of any unrecoverable errors after calling |callback|, and it is
  // guaranteed to outlive the processor. StartCallback takes a SyncError and an
  // ActivationContext; the context should be nullptr iff the error is set.
  virtual void OnSyncStarting(
      std::unique_ptr<DataTypeErrorHandler> error_handler,
      const StartCallback& callback) = 0;

  // Indicates that sync is being disabled permanently for this data type. All
  // metadata should be erased from storage.
  virtual void DisableSync() = 0;

  // Returns a boolean representing whether the processor's metadata is
  // currently up to date and accurately tracking the model type's data. If
  // false, calls to Put and Delete will no-op and can be omitted by bridge.
  virtual bool IsTrackingMetadata() = 0;

  // Report an error in the model to sync. Should be called for any persistence
  // or consistency error the bridge encounters outside of a method that allows
  // returning a ModelError directly. Outstanding callbacks are not expected to
  // be called after an error. This will result in sync being temporarily
  // disabled for the model type (generally until the next restart).
  virtual void ReportError(const ModelError& error) = 0;

  // A convenience form of the above.
  virtual void ReportError(const tracked_objects::Location& location,
                           const std::string& message) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MODEL_TYPE_CHANGE_PROCESSOR_H_
