// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/leveldb_wrapper_impl.h"

#include "base/bind.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/leveldb/public/cpp/util.h"
#include "content/public/browser/browser_thread.h"

namespace content {

bool LevelDBWrapperImpl::s_aggressive_flushing_enabled_ = false;

LevelDBWrapperImpl::RateLimiter::RateLimiter(size_t desired_rate,
                                            base::TimeDelta time_quantum)
    : rate_(desired_rate), samples_(0), time_quantum_(time_quantum) {
  DCHECK_GT(desired_rate, 0ul);
}

base::TimeDelta LevelDBWrapperImpl::RateLimiter::ComputeTimeNeeded() const {
  return time_quantum_ * (samples_ / rate_);
}

base::TimeDelta LevelDBWrapperImpl::RateLimiter::ComputeDelayNeeded(
    const base::TimeDelta elapsed_time) const {
  base::TimeDelta time_needed = ComputeTimeNeeded();
  if (time_needed > elapsed_time)
    return time_needed - elapsed_time;
  return base::TimeDelta();
}

LevelDBWrapperImpl::CommitBatch::CommitBatch() : clear_all_first(false) {}
LevelDBWrapperImpl::CommitBatch::~CommitBatch() {}

LevelDBWrapperImpl::LevelDBWrapperImpl(
    leveldb::mojom::LevelDBDatabase* database,
    const std::string& prefix,
    size_t max_size,
    base::TimeDelta default_commit_delay,
    int max_bytes_per_hour,
    int max_commits_per_hour,
    const base::Closure& no_bindings_callback,
    const PrepareToCommitCallback& prepare_to_commit_callback)
    : prefix_(leveldb::StdStringToUint8Vector(prefix)),
      no_bindings_callback_(no_bindings_callback),
      prepare_to_commit_callback_(prepare_to_commit_callback),
      database_(database),
      bytes_used_(0),
      max_size_(max_size),
      start_time_(base::TimeTicks::Now()),
      default_commit_delay_(default_commit_delay),
      data_rate_limiter_(max_bytes_per_hour, base::TimeDelta::FromHours(1)),
      commit_rate_limiter_(max_commits_per_hour, base::TimeDelta::FromHours(1)),
      weak_ptr_factory_(this) {
  bindings_.set_connection_error_handler(base::Bind(
      &LevelDBWrapperImpl::OnConnectionError, base::Unretained(this)));
}

LevelDBWrapperImpl::~LevelDBWrapperImpl() {
  if (commit_batch_)
    CommitChanges();
}

void LevelDBWrapperImpl::Bind(mojom::LevelDBWrapperRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void LevelDBWrapperImpl::EnableAggressiveCommitDelay() {
  s_aggressive_flushing_enabled_ = true;
}

void LevelDBWrapperImpl::ScheduleImmediateCommit() {
  if (!on_load_complete_tasks_.empty()) {
    LoadMap(base::Bind(&LevelDBWrapperImpl::ScheduleImmediateCommit,
                       base::Unretained(this)));
    return;
  }

  if (!database_ || !commit_batch_)
    return;
  CommitChanges();
}

void LevelDBWrapperImpl::AddObserver(
    mojom::LevelDBObserverAssociatedPtrInfo observer) {
  mojom::LevelDBObserverAssociatedPtr observer_ptr;
  observer_ptr.Bind(std::move(observer));
  observers_.AddPtr(std::move(observer_ptr));
}

void LevelDBWrapperImpl::Put(const std::vector<uint8_t>& key,
                             const std::vector<uint8_t>& value,
                             const std::string& source,
                             const PutCallback& callback) {
  if (!map_) {
    LoadMap(base::Bind(&LevelDBWrapperImpl::Put, base::Unretained(this), key,
                       value, source, callback));
    return;
  }

  bool has_old_item = false;
  size_t old_item_size = 0;
  auto found = map_->find(key);
  if (found != map_->end()) {
    if (found->second == value) {
      callback.Run(true);  // Key already has this value.
      return;
    }
    old_item_size = key.size() + found->second.size();
    has_old_item = true;
  }
  size_t new_item_size = key.size() + value.size();
  size_t new_bytes_used = bytes_used_ - old_item_size + new_item_size;

  // Only check quota if the size is increasing, this allows
  // shrinking changes to pre-existing maps that are over budget.
  if (new_item_size > old_item_size && new_bytes_used > max_size_) {
    callback.Run(false);
    return;
  }

  if (database_) {
    CreateCommitBatchIfNeeded();
    commit_batch_->changed_keys.insert(key);
  }

  std::vector<uint8_t> old_value;
  if (has_old_item) {
    old_value.swap((*map_)[key]);
  }
  (*map_)[key] = value;
  bytes_used_ = new_bytes_used;
  if (!has_old_item) {
    // We added a new key/value pair.
    observers_.ForAllPtrs(
        [&key, &value, &source](mojom::LevelDBObserver* observer) {
          observer->KeyAdded(key, value, source);
        });
  } else {
    // We changed the value for an existing key.
    observers_.ForAllPtrs(
        [&key, &value, &source, &old_value](mojom::LevelDBObserver* observer) {
          observer->KeyChanged(key, value, old_value, source);
        });
  }
  callback.Run(true);
}

void LevelDBWrapperImpl::Delete(const std::vector<uint8_t>& key,
                                const std::string& source,
                                const DeleteCallback& callback) {
  if (!map_) {
    LoadMap(base::Bind(&LevelDBWrapperImpl::Delete, base::Unretained(this), key,
                       source, callback));
    return;
  }

  auto found = map_->find(key);
  if (found == map_->end()) {
    callback.Run(true);
    return;
  }

  if (database_) {
    CreateCommitBatchIfNeeded();
    commit_batch_->changed_keys.insert(std::move(found->first));
  }

  std::vector<uint8_t> old_value(std::move(found->second));
  map_->erase(found);
  bytes_used_ -= key.size() + old_value.size();
  observers_.ForAllPtrs(
      [&key, &source, &old_value](mojom::LevelDBObserver* observer) {
        observer->KeyDeleted(key, old_value, source);
      });
  callback.Run(true);
}

void LevelDBWrapperImpl::DeleteAll(const std::string& source,
                                   const DeleteAllCallback& callback) {
  if (!map_ && !on_load_complete_tasks_.empty()) {
    LoadMap(
        base::Bind(&LevelDBWrapperImpl::DeleteAll, base::Unretained(this),
                    source, callback));
    return;
  }

  if (database_ && (!map_ || !map_->empty())) {
    CreateCommitBatchIfNeeded();
    commit_batch_->clear_all_first = true;
    commit_batch_->changed_keys.clear();
  }
  if (!map_)
    map_.reset(new ValueMap);

  map_->clear();
  bytes_used_ = 0;
  observers_.ForAllPtrs(
      [&source](mojom::LevelDBObserver* observer) {
        observer->AllDeleted(source);
      });
  callback.Run(true);
}

void LevelDBWrapperImpl::Get(const std::vector<uint8_t>& key,
                             const GetCallback& callback) {
  if (!map_) {
    LoadMap(base::Bind(&LevelDBWrapperImpl::Get, base::Unretained(this), key,
                       callback));
    return;
  }

  auto found = map_->find(key);
  if (found == map_->end()) {
    callback.Run(false, std::vector<uint8_t>());
    return;
  }
  callback.Run(true, found->second);
}

void LevelDBWrapperImpl::GetAll(
    mojom::LevelDBWrapperGetAllCallbackAssociatedPtrInfo complete_callback,
    const GetAllCallback& callback) {
  if (!map_) {
    LoadMap(base::Bind(&LevelDBWrapperImpl::GetAll, base::Unretained(this),
                       base::Passed(std::move(complete_callback)), callback));
    return;
  }

  std::vector<mojom::KeyValuePtr> all;
  for (const auto& it : (*map_)) {
    mojom::KeyValuePtr kv = mojom::KeyValue::New();
    kv->key = it.first;
    kv->value = it.second;
    all.push_back(std::move(kv));
  }
  callback.Run(leveldb::mojom::DatabaseError::OK, std::move(all));
  if (complete_callback.is_valid()) {
    mojom::LevelDBWrapperGetAllCallbackAssociatedPtr complete_ptr;
    complete_ptr.Bind(std::move(complete_callback));
    complete_ptr->Complete(true);
  }
}

void LevelDBWrapperImpl::OnConnectionError() {
  if (!bindings_.empty())
    return;
  // If any tasks are waiting for load to complete, delay calling the
  // no_bindings_callback_ until all those tasks have completed.
  if (!on_load_complete_tasks_.empty())
    return;
  no_bindings_callback_.Run();
}

void LevelDBWrapperImpl::LoadMap(const base::Closure& completion_callback) {
  DCHECK(!map_);
  on_load_complete_tasks_.push_back(completion_callback);
  if (on_load_complete_tasks_.size() > 1)
    return;

  if (!database_) {
    OnLoadComplete(leveldb::mojom::DatabaseError::IO_ERROR,
                   std::vector<leveldb::mojom::KeyValuePtr>());
    return;
  }

  // TODO(michaeln): Import from sqlite localstorage db.
  database_->GetPrefixed(prefix_,
                         base::Bind(&LevelDBWrapperImpl::OnLoadComplete,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void LevelDBWrapperImpl::OnLoadComplete(
    leveldb::mojom::DatabaseError status,
    std::vector<leveldb::mojom::KeyValuePtr> data) {
  DCHECK(!map_);
  map_.reset(new ValueMap);
  bytes_used_ = 0;
  for (auto& it : data) {
    DCHECK_GE(it->key.size(), prefix_.size());
    (*map_)[std::vector<uint8_t>(it->key.begin() + prefix_.size(),
                                 it->key.end())] = it->value;
    bytes_used_ += it->key.size() - prefix_.size() + it->value.size();
  }

  // We proceed without using a backing store, nothing will be persisted but the
  // class is functional for the lifetime of the object.
  // TODO(michaeln): Uma here or in the DB file?
  if (status != leveldb::mojom::DatabaseError::OK)
    database_ = nullptr;

  std::vector<base::Closure> tasks;
  on_load_complete_tasks_.swap(tasks);
  for (auto& task : tasks)
    task.Run();

  // We might need to call the no_bindings_callback_ here if bindings became
  // empty while waiting for load to complete.
  if (bindings_.empty())
    no_bindings_callback_.Run();
}

void LevelDBWrapperImpl::CreateCommitBatchIfNeeded() {
  if (commit_batch_)
    return;

  commit_batch_.reset(new CommitBatch());
  BrowserThread::PostAfterStartupTask(
      FROM_HERE, base::ThreadTaskRunnerHandle::Get(),
      base::Bind(&LevelDBWrapperImpl::StartCommitTimer,
                  weak_ptr_factory_.GetWeakPtr()));
}

void LevelDBWrapperImpl::StartCommitTimer() {
  if (!commit_batch_)
    return;

  // Start a timer to commit any changes that accrue in the batch, but only if
  // no commits are currently in flight. In that case the timer will be
  // started after the commits have happened.
  if (commit_batches_in_flight_)
    return;

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::Bind(&LevelDBWrapperImpl::CommitChanges,
                            weak_ptr_factory_.GetWeakPtr()),
      ComputeCommitDelay());
}

base::TimeDelta LevelDBWrapperImpl::ComputeCommitDelay() const {
  if (s_aggressive_flushing_enabled_)
    return base::TimeDelta::FromSeconds(1);

  base::TimeDelta elapsed_time = base::TimeTicks::Now() - start_time_;
  base::TimeDelta delay = std::max(
      default_commit_delay_,
      std::max(commit_rate_limiter_.ComputeDelayNeeded(elapsed_time),
               data_rate_limiter_.ComputeDelayNeeded(elapsed_time)));
  // TODO(michaeln): UMA_HISTOGRAM_LONG_TIMES("LevelDBWrapper.CommitDelay", d);
  return delay;
}

void LevelDBWrapperImpl::CommitChanges() {
  DCHECK(database_);
  DCHECK(map_);
  if (!commit_batch_)
    return;

  commit_rate_limiter_.add_samples(1);

  // Commit all our changes in a single batch.
  std::vector<leveldb::mojom::BatchedOperationPtr> operations =
      prepare_to_commit_callback_.Run(*this);
  if (commit_batch_->clear_all_first) {
    leveldb::mojom::BatchedOperationPtr item =
        leveldb::mojom::BatchedOperation::New();
    item->type = leveldb::mojom::BatchOperationType::DELETE_PREFIXED_KEY;
    item->key = prefix_;
    operations.push_back(std::move(item));
  }
  size_t data_size = 0;
  for (const auto& key: commit_batch_->changed_keys) {
    data_size += key.size();
    leveldb::mojom::BatchedOperationPtr item =
        leveldb::mojom::BatchedOperation::New();
    item->key.reserve(prefix_.size() + key.size());
    item->key.insert(item->key.end(), prefix_.begin(), prefix_.end());
    item->key.insert(item->key.end(), key.begin(), key.end());
    auto it = map_->find(key);
    if (it == map_->end()) {
      item->type = leveldb::mojom::BatchOperationType::DELETE_KEY;
    } else {
      item->type = leveldb::mojom::BatchOperationType::PUT_KEY;
      item->value = it->second;
      data_size += it->second.size();
    }
    operations.push_back(std::move(item));
  }
  commit_batch_.reset();

  data_rate_limiter_.add_samples(data_size);

  ++commit_batches_in_flight_;

  // TODO(michaeln): Currently there is no guarantee LevelDBDatabaseImp::Write
  // will run during a clean shutdown. We need that to avoid dataloss.
  database_->Write(std::move(operations),
                   base::Bind(&LevelDBWrapperImpl::OnCommitComplete,
                              weak_ptr_factory_.GetWeakPtr()));
}

void LevelDBWrapperImpl::OnCommitComplete(leveldb::mojom::DatabaseError error) {
  // TODO(michaeln): What if it fails, uma here or in the DB class?
  --commit_batches_in_flight_;
  StartCommitTimer();
}

}  // namespace content
