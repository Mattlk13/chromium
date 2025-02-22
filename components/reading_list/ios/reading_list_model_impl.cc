// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/ios/reading_list_model_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/ios/reading_list_model_storage.h"
#include "components/reading_list/ios/reading_list_pref_names.h"
#include "url/gurl.h"

ReadingListModelImpl::ReadingListModelImpl()
    : ReadingListModelImpl(nullptr, nullptr) {}

ReadingListModelImpl::ReadingListModelImpl(
    std::unique_ptr<ReadingListModelStorage> storage,
    PrefService* pref_service)
    : unread_entry_count_(0),
      read_entry_count_(0),
      unseen_entry_count_(0),
      pref_service_(pref_service),
      has_unseen_(false),
      loaded_(false),
      weak_ptr_factory_(this) {
  DCHECK(CalledOnValidThread());
  if (storage) {
    storage_layer_ = std::move(storage);
    storage_layer_->SetReadingListModel(this, this);
  } else {
    loaded_ = true;
    entries_ = base::MakeUnique<ReadingListEntries>();
  }
  has_unseen_ = GetPersistentHasUnseen();
}

ReadingListModelImpl::~ReadingListModelImpl() {}

void ReadingListModelImpl::StoreLoaded(
    std::unique_ptr<ReadingListEntries> entries) {
  DCHECK(CalledOnValidThread());
  entries_ = std::move(entries);
  for (auto& iterator : *entries_) {
    UpdateEntryStateCountersOnEntryInsertion(iterator.second);
  }
  DCHECK(read_entry_count_ + unread_entry_count_ == entries_->size());
  loaded_ = true;
  for (auto& observer : observers_)
    observer.ReadingListModelLoaded(this);
}

void ReadingListModelImpl::Shutdown() {
  DCHECK(CalledOnValidThread());
  for (auto& observer : observers_)
    observer.ReadingListModelBeingDeleted(this);
  loaded_ = false;
}

bool ReadingListModelImpl::loaded() const {
  DCHECK(CalledOnValidThread());
  return loaded_;
}

size_t ReadingListModelImpl::size() const {
  DCHECK(CalledOnValidThread());
  DCHECK(read_entry_count_ + unread_entry_count_ == entries_->size());
  if (!loaded())
    return 0;
  return entries_->size();
}

size_t ReadingListModelImpl::unread_size() const {
  DCHECK(CalledOnValidThread());
  DCHECK(read_entry_count_ + unread_entry_count_ == entries_->size());
  if (!loaded())
    return 0;
  return unread_entry_count_;
}

size_t ReadingListModelImpl::unseen_size() const {
  DCHECK(CalledOnValidThread());
  if (!loaded())
    return 0;
  return unseen_entry_count_;
}

void ReadingListModelImpl::SetUnseenFlag() {
  if (!has_unseen_) {
    has_unseen_ = true;
    if (!IsPerformingBatchUpdates()) {
      SetPersistentHasUnseen(true);
    }
  }
}

bool ReadingListModelImpl::GetLocalUnseenFlag() const {
  DCHECK(CalledOnValidThread());
  if (!loaded())
    return false;
  // If there are currently no unseen entries, return false even if has_unseen_
  // is true.
  // This is possible if the last unseen entry has be removed via sync.
  return has_unseen_ && unseen_entry_count_;
}

void ReadingListModelImpl::ResetLocalUnseenFlag() {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  has_unseen_ = false;
  if (!IsPerformingBatchUpdates())
    SetPersistentHasUnseen(false);
}

void ReadingListModelImpl::MarkAllSeen() {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  if (unseen_entry_count_ == 0) {
    return;
  }
  std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
      model_batch_updates = BeginBatchUpdates();
  for (auto& iterator : *entries_) {
    ReadingListEntry& entry = iterator.second;
    if (entry.HasBeenSeen()) {
      continue;
    }
    for (auto& observer : observers_) {
      observer.ReadingListWillUpdateEntry(this, iterator.first);
    }
    UpdateEntryStateCountersOnEntryRemoval(entry);
    entry.SetRead(false);
    UpdateEntryStateCountersOnEntryInsertion(entry);
    if (storage_layer_) {
      storage_layer_->SaveEntry(entry);
    }
    for (auto& observer : observers_) {
      observer.ReadingListDidApplyChanges(this);
    }
  }
  DCHECK(unseen_entry_count_ == 0);
}

void ReadingListModelImpl::UpdateEntryStateCountersOnEntryRemoval(
    const ReadingListEntry& entry) {
  if (!entry.HasBeenSeen()) {
    unseen_entry_count_--;
  }
  if (entry.IsRead()) {
    read_entry_count_--;
  } else {
    unread_entry_count_--;
  }
}

void ReadingListModelImpl::UpdateEntryStateCountersOnEntryInsertion(
    const ReadingListEntry& entry) {
  if (!entry.HasBeenSeen()) {
    unseen_entry_count_++;
  }
  if (entry.IsRead()) {
    read_entry_count_++;
  } else {
    unread_entry_count_++;
  }
}

const std::vector<GURL> ReadingListModelImpl::Keys() const {
  std::vector<GURL> keys;
  for (const auto& iterator : *entries_) {
    keys.push_back(iterator.first);
  }
  return keys;
}

const ReadingListEntry* ReadingListModelImpl::GetEntryByURL(
    const GURL& gurl) const {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  return GetMutableEntryFromURL(gurl);
}

const ReadingListEntry* ReadingListModelImpl::GetFirstUnreadEntry(
    bool distilled) const {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  if (unread_entry_count_ == 0) {
    return nullptr;
  }
  int64_t update_time_all = 0;
  const ReadingListEntry* first_entry_all = nullptr;
  int64_t update_time_distilled = 0;
  const ReadingListEntry* first_entry_distilled = nullptr;
  for (auto& iterator : *entries_) {
    ReadingListEntry& entry = iterator.second;
    if (entry.IsRead()) {
      continue;
    }
    if (entry.UpdateTime() > update_time_all) {
      update_time_all = entry.UpdateTime();
      first_entry_all = &entry;
    }
    if (entry.DistilledState() == ReadingListEntry::PROCESSED &&
        entry.UpdateTime() > update_time_distilled) {
      update_time_distilled = entry.UpdateTime();
      first_entry_distilled = &entry;
    }
  }
  DCHECK(first_entry_all);
  DCHECK_GT(update_time_all, 0);
  if (distilled && first_entry_distilled) {
    return first_entry_distilled;
  }
  return first_entry_all;
}

ReadingListEntry* ReadingListModelImpl::GetMutableEntryFromURL(
    const GURL& url) const {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  auto iterator = entries_->find(url);
  if (iterator == entries_->end()) {
    return nullptr;
  }
  return &(iterator->second);
}

void ReadingListModelImpl::SyncAddEntry(
    std::unique_ptr<ReadingListEntry> entry) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  // entry must not already exist.
  DCHECK(GetMutableEntryFromURL(entry->URL()) == nullptr);
  for (auto& observer : observers_)
    observer.ReadingListWillAddEntry(this, *entry);
  UpdateEntryStateCountersOnEntryInsertion(*entry);
  if (!entry->HasBeenSeen()) {
    SetUnseenFlag();
  }
  GURL url = entry->URL();
  entries_->insert(std::make_pair(url, std::move(*entry)));
  for (auto& observer : observers_) {
    observer.ReadingListDidAddEntry(this, url);
    observer.ReadingListDidApplyChanges(this);
  }
}

ReadingListEntry* ReadingListModelImpl::SyncMergeEntry(
    std::unique_ptr<ReadingListEntry> entry) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  ReadingListEntry* existing_entry = GetMutableEntryFromURL(entry->URL());
  DCHECK(existing_entry);
  GURL url = entry->URL();

  for (auto& observer : observers_)
    observer.ReadingListWillMoveEntry(this, url);

  bool was_seen = existing_entry->HasBeenSeen();
  UpdateEntryStateCountersOnEntryRemoval(*existing_entry);
  existing_entry->MergeWithEntry(*entry);
  existing_entry = GetMutableEntryFromURL(url);
  UpdateEntryStateCountersOnEntryInsertion(*existing_entry);
  if (was_seen && !existing_entry->HasBeenSeen()) {
    // Only set the flag if a new unseen entry is added.
    SetUnseenFlag();
  }
  for (auto& observer : observers_) {
    observer.ReadingListDidMoveEntry(this, url);
    observer.ReadingListDidApplyChanges(this);
  }

  return existing_entry;
}

void ReadingListModelImpl::SyncRemoveEntry(const GURL& url) {
  RemoveEntryByURLImpl(url, true);
}

void ReadingListModelImpl::RemoveEntryByURL(const GURL& url) {
  RemoveEntryByURLImpl(url, false);
}

void ReadingListModelImpl::RemoveEntryByURLImpl(const GURL& url,
                                                bool from_sync) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  const ReadingListEntry* entry = GetEntryByURL(url);
  if (!entry)
    return;

  for (auto& observer : observers_)
    observer.ReadingListWillRemoveEntry(this, url);

  if (storage_layer_ && !from_sync) {
    storage_layer_->RemoveEntry(*entry);
  }
  UpdateEntryStateCountersOnEntryRemoval(*entry);

  entries_->erase(url);
  for (auto& observer : observers_)
    observer.ReadingListDidApplyChanges(this);
}

const ReadingListEntry& ReadingListModelImpl::AddEntry(
    const GURL& url,
    const std::string& title) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  DCHECK(url.SchemeIsHTTPOrHTTPS());
  RemoveEntryByURL(url);

  std::string trimmedTitle(title);
  base::TrimWhitespaceASCII(trimmedTitle, base::TRIM_ALL, &trimmedTitle);

  ReadingListEntry entry(url, trimmedTitle);
  for (auto& observer : observers_)
    observer.ReadingListWillAddEntry(this, entry);
  UpdateEntryStateCountersOnEntryInsertion(entry);
  SetUnseenFlag();
  entries_->insert(std::make_pair(url, std::move(entry)));

  if (storage_layer_) {
    storage_layer_->SaveEntry(*GetEntryByURL(url));
  }

  for (auto& observer : observers_) {
    observer.ReadingListDidAddEntry(this, url);
    observer.ReadingListDidApplyChanges(this);
  }

  return entries_->at(url);
}

void ReadingListModelImpl::SetReadStatus(const GURL& url, bool read) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  auto iterator = entries_->find(url);
  if (iterator == entries_->end()) {
    return;
  }
  ReadingListEntry& entry = iterator->second;
  if (entry.IsRead() == read) {
    return;
  }
  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListWillMoveEntry(this, url);
  }
  UpdateEntryStateCountersOnEntryRemoval(entry);
  entry.SetRead(read);
  entry.MarkEntryUpdated();
  UpdateEntryStateCountersOnEntryInsertion(entry);

  if (storage_layer_) {
    storage_layer_->SaveEntry(entry);
  }
  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidMoveEntry(this, url);
    observer.ReadingListDidApplyChanges(this);
  }
}

void ReadingListModelImpl::SetEntryTitle(const GURL& url,
                                         const std::string& title) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  auto iterator = entries_->find(url);
  if (iterator == entries_->end()) {
    return;
  }
  ReadingListEntry& entry = iterator->second;
  if (entry.Title() == title) {
    return;
  }

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListWillUpdateEntry(this, url);
  }
  entry.SetTitle(title);
  if (storage_layer_) {
    storage_layer_->SaveEntry(entry);
  }
  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidApplyChanges(this);
  }
}

void ReadingListModelImpl::SetEntryDistilledPath(
    const GURL& url,
    const base::FilePath& distilled_path) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  auto iterator = entries_->find(url);
  if (iterator == entries_->end()) {
    return;
  }
  ReadingListEntry& entry = iterator->second;
  if (entry.DistilledState() == ReadingListEntry::PROCESSED &&
      entry.DistilledPath() == distilled_path) {
    return;
  }

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListWillUpdateEntry(this, url);
  }
  entry.SetDistilledPath(distilled_path);
  if (storage_layer_) {
    storage_layer_->SaveEntry(entry);
  }
  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidApplyChanges(this);
  }
}

void ReadingListModelImpl::SetEntryDistilledState(
    const GURL& url,
    ReadingListEntry::DistillationState state) {
  DCHECK(CalledOnValidThread());
  DCHECK(loaded());
  auto iterator = entries_->find(url);
  if (iterator == entries_->end()) {
    return;
  }
  ReadingListEntry& entry = iterator->second;
  if (entry.DistilledState() == state) {
    return;
  }

  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListWillUpdateEntry(this, url);
  }
  entry.SetDistilledState(state);
  if (storage_layer_) {
    storage_layer_->SaveEntry(entry);
  }
  for (ReadingListModelObserver& observer : observers_) {
    observer.ReadingListDidApplyChanges(this);
  }
}

std::unique_ptr<ReadingListModel::ScopedReadingListBatchUpdate>
ReadingListModelImpl::CreateBatchToken() {
  return base::MakeUnique<ReadingListModelImpl::ScopedReadingListBatchUpdate>(
      this);
}

ReadingListModelImpl::ScopedReadingListBatchUpdate::
    ScopedReadingListBatchUpdate(ReadingListModelImpl* model)
    : ReadingListModel::ScopedReadingListBatchUpdate::
          ScopedReadingListBatchUpdate(model) {
  if (model->StorageLayer()) {
    storage_token_ = model->StorageLayer()->EnsureBatchCreated();
  }
}

ReadingListModelImpl::ScopedReadingListBatchUpdate::
    ~ScopedReadingListBatchUpdate() {
  storage_token_.reset();
}

void ReadingListModelImpl::LeavingBatchUpdates() {
  DCHECK(CalledOnValidThread());
  if (storage_layer_) {
    SetPersistentHasUnseen(has_unseen_);
  }
  ReadingListModel::LeavingBatchUpdates();
}

void ReadingListModelImpl::EnteringBatchUpdates() {
  DCHECK(CalledOnValidThread());
  ReadingListModel::EnteringBatchUpdates();
}

void ReadingListModelImpl::SetPersistentHasUnseen(bool has_unseen) {
  DCHECK(CalledOnValidThread());
  if (!pref_service_) {
    return;
  }
  pref_service_->SetBoolean(reading_list::prefs::kReadingListHasUnseenEntries,
                            has_unseen);
}

bool ReadingListModelImpl::GetPersistentHasUnseen() {
  DCHECK(CalledOnValidThread());
  if (!pref_service_) {
    return false;
  }
  return pref_service_->GetBoolean(
      reading_list::prefs::kReadingListHasUnseenEntries);
}

syncer::ModelTypeSyncBridge* ReadingListModelImpl::GetModelTypeSyncBridge() {
  DCHECK(loaded());
  if (!storage_layer_)
    return nullptr;
  return storage_layer_.get();
}

ReadingListModelStorage* ReadingListModelImpl::StorageLayer() {
  return storage_layer_.get();
}
