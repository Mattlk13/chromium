// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_TEST_ARCHIVER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_TEST_ARCHIVER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string16.h"
#include "components/offline_pages/core/offline_page_archiver.h"

class GURL;

namespace base {
class FilePath;
}  // namespace

namespace offline_pages {

// A test archiver class, which allows for testing offline pages without a need
// for an actual web contents.
class OfflinePageTestArchiver : public OfflinePageArchiver {
 public:
  // TODO(fgorski): Try refactoring the observer out and replace it with a
  // callback, or completely remove the call to |SetLastPathCreatedByArchiver|.
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void SetLastPathCreatedByArchiver(
        const base::FilePath& file_path) = 0;
  };

  OfflinePageTestArchiver(
      Observer* observer,
      const GURL& url,
      ArchiverResult result,
      const base::string16& result_title,
      int64_t size_to_report,
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner);
  ~OfflinePageTestArchiver() override;

  // OfflinePageArchiver implementation:
  void CreateArchive(const base::FilePath& archives_dir,
                     int64_t archive_id,
                     const CreateArchiveCallback& callback) override;

  // Completes the creation of archive. Should be used with |set_delayed| set to
  // true.
  void CompleteCreateArchive();

  // When set to true, |CompleteCreateArchive| should be called explicitly for
  // the process to finish.
  // TODO(fgorski): See if we can move this to the constructor.
  void set_delayed(bool delayed) { delayed_ = delayed; }

  // Allows to explicitly specify a file name for the tests.
  // TODO(fgorski): See if we can move this to the constructor.
  void set_filename(const base::FilePath& filename) { filename_ = filename; }

  bool create_archive_called() const { return create_archive_called_; }

 private:
  // Not owned. Outlives OfflinePageTestArchiver.
  Observer* observer_;
  GURL url_;
  base::FilePath archives_dir_;
  base::FilePath filename_;
  ArchiverResult result_;
  int64_t size_to_report_;
  bool create_archive_called_;
  bool delayed_;
  base::string16 result_title_;
  CreateArchiveCallback callback_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageTestArchiver);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_TEST_ARCHIVER_H_
