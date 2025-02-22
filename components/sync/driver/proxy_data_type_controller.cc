// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/proxy_data_type_controller.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "components/sync/model/sync_merge_result.h"

namespace syncer {

ProxyDataTypeController::ProxyDataTypeController(ModelType type)
    : DataTypeController(type, base::Closure()), state_(NOT_RUNNING) {
  DCHECK(ProxyTypes().Has(type));
}

ProxyDataTypeController::~ProxyDataTypeController() {}

bool ProxyDataTypeController::ShouldLoadModelBeforeConfigure() const {
  return false;
}

void ProxyDataTypeController::LoadModels(
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  state_ = MODEL_LOADED;
  model_load_callback.Run(type(), SyncError());
}

void ProxyDataTypeController::RegisterWithBackend(
    base::Callback<void(bool)> set_downloaded,
    ModelTypeConfigurer* configurer) {}

void ProxyDataTypeController::StartAssociating(
    const StartCallback& start_callback) {
  DCHECK(CalledOnValidThread());
  SyncMergeResult local_merge_result(type());
  SyncMergeResult syncer_merge_result(type());
  state_ = RUNNING;
  start_callback.Run(DataTypeController::OK, local_merge_result,
                     syncer_merge_result);
}

void ProxyDataTypeController::Stop() {
  state_ = NOT_RUNNING;
}

std::string ProxyDataTypeController::name() const {
  // For logging only.
  return ModelTypeToString(type());
}

DataTypeController::State ProxyDataTypeController::state() const {
  return state_;
}

void ProxyDataTypeController::ActivateDataType(
    ModelTypeConfigurer* configurer) {}

void ProxyDataTypeController::DeactivateDataType(
    ModelTypeConfigurer* configurer) {}

void ProxyDataTypeController::GetAllNodes(const AllNodesCallback& callback) {
  callback.Run(type(), base::MakeUnique<base::ListValue>());
}

void ProxyDataTypeController::GetStatusCounters(
    const StatusCountersCallback& callback) {
  syncer::StatusCounters counters;
  callback.Run(type(), counters);
}

std::unique_ptr<DataTypeErrorHandler>
ProxyDataTypeController::CreateErrorHandler() {
  NOTREACHED();
  return nullptr;
}

}  // namespace syncer
