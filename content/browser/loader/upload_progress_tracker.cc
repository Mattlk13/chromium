// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/upload_progress_tracker.h"

#include "base/logging.h"
#include "net/base/upload_progress.h"
#include "net/url_request/url_request.h"

namespace content {
namespace {
// The interval for calls to ReportUploadProgress.
constexpr base::TimeDelta kUploadProgressInterval =
    base::TimeDelta::FromMilliseconds(100);
}  // namespace

UploadProgressTracker::UploadProgressTracker(
    const tracked_objects::Location& location,
    UploadProgressReportCallback report_progress,
    net::URLRequest* request)
    : request_(request), report_progress_(std::move(report_progress)) {
  DCHECK(request_);
  DCHECK(report_progress_);

  progress_timer_.Start(location, kUploadProgressInterval, this,
                        &UploadProgressTracker::ReportUploadProgressIfNeeded);
}

UploadProgressTracker::~UploadProgressTracker() {}

void UploadProgressTracker::OnAckReceived() {
  waiting_for_upload_progress_ack_ = false;
}

void UploadProgressTracker::OnUploadCompleted() {
  waiting_for_upload_progress_ack_ = false;
  ReportUploadProgressIfNeeded();
  progress_timer_.Stop();
}

void UploadProgressTracker::ReportUploadProgressIfNeeded() {
  if (waiting_for_upload_progress_ack_)
    return;

  net::UploadProgress progress = request_->GetUploadProgress();
  if (!progress.size())
    return;  // Nothing to upload.

  if (progress.position() == last_upload_position_)
    return;  // No progress made since last time.

  const uint64_t kHalfPercentIncrements = 200;
  const base::TimeDelta kOneSecond = base::TimeDelta::FromMilliseconds(1000);

  uint64_t amt_since_last = progress.position() - last_upload_position_;
  base::TimeDelta time_since_last = base::TimeTicks::Now() - last_upload_ticks_;

  bool is_finished = (progress.size() == progress.position());
  bool enough_new_progress =
      (amt_since_last > (progress.size() / kHalfPercentIncrements));
  bool too_much_time_passed = time_since_last > kOneSecond;

  if (is_finished || enough_new_progress || too_much_time_passed) {
    report_progress_.Run(progress.position(), progress.size());
    waiting_for_upload_progress_ack_ = true;
    last_upload_ticks_ = base::TimeTicks::Now();
    last_upload_position_ = progress.position();
  }
}

}  // namespace content
