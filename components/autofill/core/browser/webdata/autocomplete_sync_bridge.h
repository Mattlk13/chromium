// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_SYNC_BRIDGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_SYNC_BRIDGE_H_

#include <memory>
#include <string>

#include "base/scoped_observer.h"
#include "base/supports_user_data.h"
#include "base/threading/non_thread_safe.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace autofill {

class AutofillTable;
class AutofillWebDataBackend;
class AutofillWebDataService;

class AutocompleteSyncBridge : public base::SupportsUserData::Data,
                               public syncer::ModelTypeSyncBridge,
                               public AutofillWebDataServiceObserverOnDBThread {
 public:
  AutocompleteSyncBridge();
  AutocompleteSyncBridge(
      AutofillWebDataBackend* backend,
      const ChangeProcessorFactory& change_processor_factory);
  ~AutocompleteSyncBridge() override;

  static void CreateForWebDataServiceAndBackend(
      AutofillWebDataService* web_data_service,
      AutofillWebDataBackend* web_data_backend);

  static AutocompleteSyncBridge* FromWebDataService(
      AutofillWebDataService* web_data_service);

  // syncer::ModelTypeService implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  syncer::ModelError MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityDataMap entity_data_map) override;
  syncer::ModelError ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllData(DataCallback callback) override;
  // Generate a tag that uniquely identifies |entity_data| across all data
  // types. This is used to identify the entity on the server. The format, which
  // must remain the same for server compatibility, is:
  // "autofill_entry|$name|$value" where $name and $value are escaped.
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  // Generate a string key uniquely identifying |entity_data| in the context of
  // local storage. The format, which should stay the same, is $name|$value"
  // where $name and $value are escaped.
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

  // AutofillWebDataServiceObserverOnDBThread implementation.
  void AutofillEntriesChanged(const AutofillChangeList& changes) override;

  static AutofillEntry CreateAutofillEntry(
      const sync_pb::AutofillSpecifics& autofill_specifics);

 private:
  // Returns the table associated with the |web_data_backend_|.
  AutofillTable* GetAutofillTable() const;

  std::string GetStorageKeyFromAutofillEntry(
      const autofill::AutofillEntry& entry);

  static std::string FormatStorageKey(const std::string& name,
                                      const std::string& value);

  base::ThreadChecker thread_checker_;

  // AutocompleteSyncBridge is owned by |web_data_backend_| through
  // SupportsUserData, so it's guaranteed to outlive |this|.
  AutofillWebDataBackend* const web_data_backend_;

  ScopedObserver<AutofillWebDataBackend, AutocompleteSyncBridge>
      scoped_observer_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_SYNC_BRIDGE_H_
