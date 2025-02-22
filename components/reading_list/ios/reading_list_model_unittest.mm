// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reading_list/ios/reading_list_model.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#import "base/test/ios/wait_util.h"
#include "components/reading_list/ios/reading_list_model_impl.h"
#include "components/reading_list/ios/reading_list_model_storage.h"
#include "components/reading_list/ios/reading_list_store_delegate.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const GURL callback_url("http://example.com");
const std::string callback_title("test title");

class TestReadingListStorageObserver {
 public:
  virtual void ReadingListDidSaveEntry() = 0;
  virtual void ReadingListDidRemoveEntry() = 0;
};

class TestReadingListStorage : public ReadingListModelStorage {
 public:
  TestReadingListStorage(TestReadingListStorageObserver* observer)
      : ReadingListModelStorage(
            base::Bind(&syncer::ModelTypeChangeProcessor::Create),
            syncer::READING_LIST),
        entries_(new ReadingListStoreDelegate::ReadingListEntries()),
        observer_(observer) {}

  void AddSampleEntries() {
    // Adds timer and interlace read/unread entry creation to avoid having two
    // entries with the same creation timestamp.
    ReadingListEntry unread_a(GURL("http://unread_a.com"), "unread_a");
    entries_->insert(
        std::make_pair(GURL("http://unread_a.com"), std::move(unread_a)));
    base::test::ios::SpinRunLoopWithMinDelay(
        base::TimeDelta::FromMilliseconds(5));

    ReadingListEntry read_a(GURL("http://read_a.com"), "read_a");
    read_a.SetRead(true);
    entries_->insert(
        std::make_pair(GURL("http://read_a.com"), std::move(read_a)));
    base::test::ios::SpinRunLoopWithMinDelay(
        base::TimeDelta::FromMilliseconds(5));

    ReadingListEntry unread_b(GURL("http://unread_b.com"), "unread_b");
    entries_->insert(
        std::make_pair(GURL("http://unread_b.com"), std::move(unread_b)));
    base::test::ios::SpinRunLoopWithMinDelay(
        base::TimeDelta::FromMilliseconds(5));

    ReadingListEntry read_b(GURL("http://read_b.com"), "read_b");
    read_b.SetRead(true);
    entries_->insert(
        std::make_pair(GURL("http://read_b.com"), std::move(read_b)));
    base::test::ios::SpinRunLoopWithMinDelay(
        base::TimeDelta::FromMilliseconds(5));

    ReadingListEntry unread_c(GURL("http://unread_c.com"), "unread_c");
    entries_->insert(
        std::make_pair(GURL("http://unread_c.com"), std::move(unread_c)));
    base::test::ios::SpinRunLoopWithMinDelay(
        base::TimeDelta::FromMilliseconds(5));

    ReadingListEntry read_c(GURL("http://read_c.com"), "read_c");
    read_c.SetRead(true);
    entries_->insert(
        std::make_pair(GURL("http://read_c.com"), std::move(read_c)));
    base::test::ios::SpinRunLoopWithMinDelay(
        base::TimeDelta::FromMilliseconds(5));

    ReadingListEntry unread_d(GURL("http://unread_d.com"), "unread_d");
    entries_->insert(
        std::make_pair(GURL("http://unread_d.com"), std::move(unread_d)));
    base::test::ios::SpinRunLoopWithMinDelay(
        base::TimeDelta::FromMilliseconds(5));
  }

  void SetReadingListModel(ReadingListModel* model,
                           ReadingListStoreDelegate* delegate) override {
    delegate->StoreLoaded(std::move(entries_));
  }

  // Saves or updates an entry. If the entry is not yet in the database, it is
  // created.
  void SaveEntry(const ReadingListEntry& entry) override {
    observer_->ReadingListDidSaveEntry();
  }

  // Removes an entry from the storage.
  void RemoveEntry(const ReadingListEntry& entry) override {
    observer_->ReadingListDidRemoveEntry();
  }

  std::unique_ptr<ScopedBatchUpdate> EnsureBatchCreated() override {
    return std::unique_ptr<ScopedBatchUpdate>();
  }

  // Syncing is not used in this test class.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override {
    NOTREACHED();
    return std::unique_ptr<syncer::MetadataChangeList>();
  }

  syncer::ModelError MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityDataMap entity_data_map) override {
    NOTREACHED();
    return syncer::ModelError();
  }

  syncer::ModelError ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override {
    NOTREACHED();
    return syncer::ModelError();
  }

  void GetData(StorageKeyList storage_keys, DataCallback callback) override {
    NOTREACHED();
    return;
  }

  void GetAllData(DataCallback callback) override {
    NOTREACHED();
    return;
  }

  std::string GetClientTag(const syncer::EntityData& entity_data) override {
    NOTREACHED();
    return "";
  }

  std::string GetStorageKey(const syncer::EntityData& entity_data) override {
    NOTREACHED();
    return "";
  }

 private:
  std::unique_ptr<ReadingListStoreDelegate::ReadingListEntries> entries_;
  TestReadingListStorageObserver* observer_;
};

class ReadingListModelTest : public ReadingListModelObserver,
                             public TestReadingListStorageObserver,
                             public testing::Test {
 public:
  ReadingListModelTest()
      : callback_called_(false), model_(new ReadingListModelImpl()) {
    ClearCounts();
    model_->AddObserver(this);
  }
  ~ReadingListModelTest() override {}

  void SetStorage(std::unique_ptr<TestReadingListStorage> storage) {
    model_ =
        base::MakeUnique<ReadingListModelImpl>(std::move(storage), nullptr);
    ClearCounts();
    model_->AddObserver(this);
  }

  void ClearCounts() {
    observer_loaded_ = observer_started_batch_update_ =
        observer_completed_batch_update_ = observer_deleted_ =
            observer_remove_ = observer_move_ = observer_add_ =
                observer_did_add_ = observer_update_ = observer_did_apply_ =
                    storage_saved_ = storage_removed_ = 0;
  }

  void AssertObserverCount(int observer_loaded,
                           int observer_started_batch_update,
                           int observer_completed_batch_update,
                           int observer_deleted,
                           int observer_remove,
                           int observer_move,
                           int observer_add,
                           int observer_update,
                           int observer_did_apply) {
    ASSERT_EQ(observer_loaded, observer_loaded_);
    ASSERT_EQ(observer_started_batch_update, observer_started_batch_update_);
    ASSERT_EQ(observer_completed_batch_update,
              observer_completed_batch_update_);
    ASSERT_EQ(observer_deleted, observer_deleted_);
    ASSERT_EQ(observer_remove, observer_remove_);
    ASSERT_EQ(observer_move, observer_move_);
    // Add and did_add should be the same.
    ASSERT_EQ(observer_add, observer_add_);
    ASSERT_EQ(observer_add, observer_did_add_);
    ASSERT_EQ(observer_update, observer_update_);
    ASSERT_EQ(observer_did_apply, observer_did_apply_);
  }

  void AssertStorageCount(int storage_saved, int storage_removed) {
    ASSERT_EQ(storage_saved, storage_saved_);
    ASSERT_EQ(storage_removed, storage_removed_);
  }

  // ReadingListModelObserver
  void ReadingListModelLoaded(const ReadingListModel* model) override {
    observer_loaded_ += 1;
  }
  void ReadingListModelBeganBatchUpdates(
      const ReadingListModel* model) override {
    observer_started_batch_update_ += 1;
  }
  void ReadingListModelCompletedBatchUpdates(
      const ReadingListModel* model) override {
    observer_completed_batch_update_ += 1;
  }
  void ReadingListModelBeingDeleted(const ReadingListModel* model) override {
    observer_deleted_ += 1;
  }
  void ReadingListWillRemoveEntry(const ReadingListModel* model,
                                  const GURL& url) override {
    observer_remove_ += 1;
  }
  void ReadingListWillMoveEntry(const ReadingListModel* model,
                                const GURL& url) override {
    observer_move_ += 1;
  }
  void ReadingListWillAddEntry(const ReadingListModel* model,
                               const ReadingListEntry& entry) override {
    observer_add_ += 1;
  }
  void ReadingListDidAddEntry(const ReadingListModel* model,
                              const GURL& url) override {
    observer_did_add_ += 1;
  }
  void ReadingListWillUpdateEntry(const ReadingListModel* model,
                                  const GURL& url) override {
    observer_update_ += 1;
  }
  void ReadingListDidApplyChanges(ReadingListModel* model) override {
    observer_did_apply_ += 1;
  }

  void ReadingListDidSaveEntry() override { storage_saved_ += 1; }
  void ReadingListDidRemoveEntry() override { storage_removed_ += 1; }

  size_t UnreadSize() {
    size_t size = 0;
    for (const auto& url : model_->Keys()) {
      const ReadingListEntry* entry = model_->GetEntryByURL(url);
      if (!entry->IsRead()) {
        size++;
      }
    }
    DCHECK_EQ(size, model_->unread_size());
    return size;
  }

  size_t ReadSize() {
    size_t size = 0;
    for (const auto& url : model_->Keys()) {
      const ReadingListEntry* entry = model_->GetEntryByURL(url);
      if (entry->IsRead()) {
        size++;
      }
    }
    return size;
  }

  void Callback(const ReadingListEntry& entry) {
    EXPECT_EQ(callback_url, entry.URL());
    EXPECT_EQ(callback_title, entry.Title());
    callback_called_ = true;
  }

  bool CallbackCalled() { return callback_called_; }

 protected:
  int observer_loaded_;
  int observer_started_batch_update_;
  int observer_completed_batch_update_;
  int observer_deleted_;
  int observer_remove_;
  int observer_move_;
  int observer_add_;
  int observer_did_add_;
  int observer_update_;
  int observer_did_apply_;
  int storage_saved_;
  int storage_removed_;
  bool callback_called_;

  std::unique_ptr<ReadingListModelImpl> model_;
};

TEST_F(ReadingListModelTest, EmptyLoaded) {
  EXPECT_TRUE(model_->loaded());
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  model_->Shutdown();
  EXPECT_FALSE(model_->loaded());
  AssertObserverCount(1, 0, 0, 1, 0, 0, 0, 0, 0);
}

TEST_F(ReadingListModelTest, ModelLoaded) {
  ClearCounts();
  auto storage = base::MakeUnique<TestReadingListStorage>(this);
  storage->AddSampleEntries();
  SetStorage(std::move(storage));

  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0);
  std::map<GURL, std::string> loaded_entries;
  int size = 0;
  for (const auto& url : model_->Keys()) {
    size++;
    const ReadingListEntry* entry = model_->GetEntryByURL(url);
    loaded_entries[url] = entry->Title();
  }
  EXPECT_EQ(size, 7);
  EXPECT_EQ(loaded_entries[GURL("http://unread_a.com")], "unread_a");
  EXPECT_EQ(loaded_entries[GURL("http://unread_b.com")], "unread_b");
  EXPECT_EQ(loaded_entries[GURL("http://unread_c.com")], "unread_c");
  EXPECT_EQ(loaded_entries[GURL("http://unread_d.com")], "unread_d");
  EXPECT_EQ(loaded_entries[GURL("http://read_a.com")], "read_a");
  EXPECT_EQ(loaded_entries[GURL("http://read_b.com")], "read_b");
  EXPECT_EQ(loaded_entries[GURL("http://read_c.com")], "read_c");
}

TEST_F(ReadingListModelTest, AddEntry) {
  auto storage = base::MakeUnique<TestReadingListStorage>(this);
  SetStorage(std::move(storage));
  ClearCounts();

  const ReadingListEntry& entry =
      model_->AddEntry(GURL("http://example.com"), "\n  \tsample Test ");
  EXPECT_EQ(GURL("http://example.com"), entry.URL());
  EXPECT_EQ("sample Test", entry.Title());

  AssertObserverCount(0, 0, 0, 0, 0, 0, 1, 0, 1);
  AssertStorageCount(1, 0);
  EXPECT_EQ(1ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  EXPECT_TRUE(model_->GetLocalUnseenFlag());

  const ReadingListEntry* other_entry =
      model_->GetEntryByURL(GURL("http://example.com"));
  EXPECT_NE(other_entry, nullptr);
  EXPECT_FALSE(other_entry->IsRead());
  EXPECT_EQ(GURL("http://example.com"), other_entry->URL());
  EXPECT_EQ("sample Test", other_entry->Title());
}

TEST_F(ReadingListModelTest, SyncAddEntry) {
  auto storage = base::MakeUnique<TestReadingListStorage>(this);
  SetStorage(std::move(storage));
  auto entry =
      base::MakeUnique<ReadingListEntry>(GURL("http://example.com"), "sample");
  entry->SetRead(true);
  ClearCounts();

  model_->SyncAddEntry(std::move(entry));
  AssertObserverCount(0, 0, 0, 0, 0, 0, 1, 0, 1);
  AssertStorageCount(0, 0);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(1ul, ReadSize());
  ClearCounts();
}

TEST_F(ReadingListModelTest, SyncMergeEntry) {
  auto storage = base::MakeUnique<TestReadingListStorage>(this);
  SetStorage(std::move(storage));
  model_->AddEntry(GURL("http://example.com"), "sample");
  model_->SetEntryDistilledPath(GURL("http://example.com"),
                                base::FilePath("distilled/page.html"));
  const ReadingListEntry* local_entry =
      model_->GetEntryByURL(GURL("http://example.com"));
  int64_t local_update_time = local_entry->UpdateTime();

  base::test::ios::SpinRunLoopWithMinDelay(
      base::TimeDelta::FromMilliseconds(10));
  auto sync_entry =
      base::MakeUnique<ReadingListEntry>(GURL("http://example.com"), "sample");
  sync_entry->SetRead(true);
  ASSERT_GT(sync_entry->UpdateTime(), local_update_time);
  int64_t sync_update_time = sync_entry->UpdateTime();
  EXPECT_TRUE(sync_entry->DistilledPath().empty());

  EXPECT_EQ(1ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());

  ReadingListEntry* merged_entry =
      model_->SyncMergeEntry(std::move(sync_entry));
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(1ul, ReadSize());
  EXPECT_EQ(merged_entry->DistilledPath(),
            base::FilePath("distilled/page.html"));
  EXPECT_EQ(merged_entry->UpdateTime(), sync_update_time);
}

TEST_F(ReadingListModelTest, RemoveEntryByUrl) {
  auto storage = base::MakeUnique<TestReadingListStorage>(this);
  SetStorage(std::move(storage));
  model_->AddEntry(GURL("http://example.com"), "sample");
  ClearCounts();
  EXPECT_NE(model_->GetEntryByURL(GURL("http://example.com")), nullptr);
  EXPECT_EQ(1ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  model_->RemoveEntryByURL(GURL("http://example.com"));
  AssertObserverCount(0, 0, 0, 0, 1, 0, 0, 0, 1);
  AssertStorageCount(0, 1);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  EXPECT_EQ(model_->GetEntryByURL(GURL("http://example.com")), nullptr);

  model_->AddEntry(GURL("http://example.com"), "sample");
  model_->SetReadStatus(GURL("http://example.com"), true);
  ClearCounts();
  EXPECT_NE(model_->GetEntryByURL(GURL("http://example.com")), nullptr);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(1ul, ReadSize());
  model_->RemoveEntryByURL(GURL("http://example.com"));
  AssertObserverCount(0, 0, 0, 0, 1, 0, 0, 0, 1);
  AssertStorageCount(0, 1);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  EXPECT_EQ(model_->GetEntryByURL(GURL("http://example.com")), nullptr);
}

TEST_F(ReadingListModelTest, RemoveSyncEntryByUrl) {
  auto storage = base::MakeUnique<TestReadingListStorage>(this);
  SetStorage(std::move(storage));
  model_->AddEntry(GURL("http://example.com"), "sample");
  ClearCounts();
  EXPECT_NE(model_->GetEntryByURL(GURL("http://example.com")), nullptr);
  EXPECT_EQ(1ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  model_->SyncRemoveEntry(GURL("http://example.com"));
  AssertObserverCount(0, 0, 0, 0, 1, 0, 0, 0, 1);
  AssertStorageCount(0, 0);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  EXPECT_EQ(model_->GetEntryByURL(GURL("http://example.com")), nullptr);

  model_->AddEntry(GURL("http://example.com"), "sample");
  model_->SetReadStatus(GURL("http://example.com"), true);
  ClearCounts();
  EXPECT_NE(model_->GetEntryByURL(GURL("http://example.com")), nullptr);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(1ul, ReadSize());
  model_->SyncRemoveEntry(GURL("http://example.com"));
  AssertObserverCount(0, 0, 0, 0, 1, 0, 0, 0, 1);
  AssertStorageCount(0, 0);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  EXPECT_EQ(model_->GetEntryByURL(GURL("http://example.com")), nullptr);
}

TEST_F(ReadingListModelTest, ReadEntry) {
  model_->AddEntry(GURL("http://example.com"), "sample");

  ClearCounts();
  model_->SetReadStatus(GURL("http://example.com"), true);
  AssertObserverCount(0, 0, 0, 0, 0, 1, 0, 0, 1);
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(1ul, ReadSize());
  EXPECT_EQ(0ul, model_->unseen_size());

  const ReadingListEntry* other_entry =
      model_->GetEntryByURL(GURL("http://example.com"));
  EXPECT_NE(other_entry, nullptr);
  EXPECT_TRUE(other_entry->IsRead());
  EXPECT_EQ(GURL("http://example.com"), other_entry->URL());
  EXPECT_EQ("sample", other_entry->Title());
}

TEST_F(ReadingListModelTest, EntryFromURL) {
  GURL url1("http://example.com");
  GURL url2("http://example2.com");
  std::string entry1_title = "foo bar qux";
  model_->AddEntry(url1, entry1_title);

  // Check call with nullptr |read| parameter.
  const ReadingListEntry* entry1 = model_->GetEntryByURL(url1);
  EXPECT_NE(nullptr, entry1);
  EXPECT_EQ(entry1_title, entry1->Title());

  entry1 = model_->GetEntryByURL(url1);
  EXPECT_NE(nullptr, entry1);
  EXPECT_EQ(entry1_title, entry1->Title());
  EXPECT_EQ(entry1->IsRead(), false);
  model_->SetReadStatus(url1, true);
  entry1 = model_->GetEntryByURL(url1);
  EXPECT_NE(nullptr, entry1);
  EXPECT_EQ(entry1_title, entry1->Title());
  EXPECT_EQ(entry1->IsRead(), true);

  const ReadingListEntry* entry2 = model_->GetEntryByURL(url2);
  EXPECT_EQ(nullptr, entry2);
}

TEST_F(ReadingListModelTest, UnreadEntry) {
  // Setup.
  model_->AddEntry(GURL("http://example.com"), "sample");
  EXPECT_TRUE(model_->GetLocalUnseenFlag());
  model_->SetReadStatus(GURL("http://example.com"), true);
  ClearCounts();
  EXPECT_EQ(0ul, UnreadSize());
  EXPECT_EQ(1ul, ReadSize());
  EXPECT_FALSE(model_->GetLocalUnseenFlag());

  // Action.
  model_->SetReadStatus(GURL("http://example.com"), false);

  // Tests.
  AssertObserverCount(0, 0, 0, 0, 0, 1, 0, 0, 1);
  EXPECT_EQ(1ul, UnreadSize());
  EXPECT_EQ(0ul, ReadSize());
  EXPECT_FALSE(model_->GetLocalUnseenFlag());

  const ReadingListEntry* other_entry =
      model_->GetEntryByURL(GURL("http://example.com"));
  EXPECT_NE(other_entry, nullptr);
  EXPECT_FALSE(other_entry->IsRead());
  EXPECT_EQ(GURL("http://example.com"), other_entry->URL());
  EXPECT_EQ("sample", other_entry->Title());
}

TEST_F(ReadingListModelTest, BatchUpdates) {
  auto token = model_->BeginBatchUpdates();
  AssertObserverCount(1, 1, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  delete token.release();
  AssertObserverCount(1, 1, 1, 0, 0, 0, 0, 0, 0);
  EXPECT_FALSE(model_->IsPerformingBatchUpdates());
}

TEST_F(ReadingListModelTest, BatchUpdatesReentrant) {
  // When two updates happen at the same time, the notification is only sent
  // for beginning of first update and completion of last update.
  EXPECT_FALSE(model_->IsPerformingBatchUpdates());

  auto token = model_->BeginBatchUpdates();
  AssertObserverCount(1, 1, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  auto second_token = model_->BeginBatchUpdates();
  AssertObserverCount(1, 1, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  delete token.release();
  AssertObserverCount(1, 1, 0, 0, 0, 0, 0, 0, 0);
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  delete second_token.release();
  AssertObserverCount(1, 1, 1, 0, 0, 0, 0, 0, 0);
  EXPECT_FALSE(model_->IsPerformingBatchUpdates());

  // Consequent updates send notifications.
  auto third_token = model_->BeginBatchUpdates();
  AssertObserverCount(1, 2, 1, 0, 0, 0, 0, 0, 0);
  EXPECT_TRUE(model_->IsPerformingBatchUpdates());

  delete third_token.release();
  AssertObserverCount(1, 2, 2, 0, 0, 0, 0, 0, 0);
  EXPECT_FALSE(model_->IsPerformingBatchUpdates());
}

TEST_F(ReadingListModelTest, UpdateEntryTitle) {
  const GURL gurl("http://example.com");
  const ReadingListEntry& entry = model_->AddEntry(gurl, "sample");
  ClearCounts();

  model_->SetEntryTitle(gurl, "ping");
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 1, 1);
  EXPECT_EQ("ping", entry.Title());
}

TEST_F(ReadingListModelTest, UpdateEntryDistilledState) {
  const GURL gurl("http://example.com");
  const ReadingListEntry& entry = model_->AddEntry(gurl, "sample");
  ClearCounts();

  model_->SetEntryDistilledState(gurl, ReadingListEntry::PROCESSING);
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 1, 1);
  EXPECT_EQ(ReadingListEntry::PROCESSING, entry.DistilledState());
}

TEST_F(ReadingListModelTest, UpdateDistilledPath) {
  const GURL gurl("http://example.com");
  const ReadingListEntry& entry = model_->AddEntry(gurl, "sample");
  ClearCounts();

  model_->SetEntryDistilledPath(gurl, base::FilePath("distilled/page.html"));
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 1, 1);
  EXPECT_EQ(ReadingListEntry::PROCESSED, entry.DistilledState());
  EXPECT_EQ(base::FilePath("distilled/page.html"), entry.DistilledPath());
}

TEST_F(ReadingListModelTest, UpdateReadEntryTitle) {
  const GURL gurl("http://example.com");
  model_->AddEntry(gurl, "sample");
  model_->SetReadStatus(gurl, true);
  const ReadingListEntry* entry = model_->GetEntryByURL(gurl);
  ClearCounts();

  model_->SetEntryTitle(gurl, "ping");
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 1, 1);
  EXPECT_EQ("ping", entry->Title());
}

TEST_F(ReadingListModelTest, UpdateReadEntryState) {
  const GURL gurl("http://example.com");
  model_->AddEntry(gurl, "sample");
  model_->SetReadStatus(gurl, true);
  const ReadingListEntry* entry = model_->GetEntryByURL(gurl);
  ClearCounts();

  model_->SetEntryDistilledState(gurl, ReadingListEntry::PROCESSING);
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 1, 1);
  EXPECT_EQ(ReadingListEntry::PROCESSING, entry->DistilledState());
}

TEST_F(ReadingListModelTest, UpdateReadDistilledPath) {
  const GURL gurl("http://example.com");
  model_->AddEntry(gurl, "sample");
  model_->SetReadStatus(gurl, true);
  const ReadingListEntry* entry = model_->GetEntryByURL(gurl);
  ClearCounts();

  model_->SetEntryDistilledPath(gurl, base::FilePath("distilled/page.html"));
  AssertObserverCount(0, 0, 0, 0, 0, 0, 0, 1, 1);
  EXPECT_EQ(ReadingListEntry::PROCESSED, entry->DistilledState());
  EXPECT_EQ(base::FilePath("distilled/page.html"), entry->DistilledPath());
}

// Tests that ReadingListModel calls CallbackModelBeingDeleted when destroyed.
TEST_F(ReadingListModelTest, CallbackModelBeingDeleted) {
  AssertObserverCount(1, 0, 0, 0, 0, 0, 0, 0, 0);
  model_.reset();
  AssertObserverCount(1, 0, 0, 1, 0, 0, 0, 0, 0);
}

}  // namespace
