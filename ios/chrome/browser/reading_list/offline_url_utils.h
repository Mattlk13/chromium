// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_OFFLINE_URL_UTILS_H_
#define IOS_CHROME_BROWSER_READING_LIST_OFFLINE_URL_UTILS_H_

#include <string>

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "url/gurl.h"

namespace reading_list {

// The distilled URL chrome://offline/... that will load the file at |path|.
GURL DistilledURLForPath(const base::FilePath& path, const GURL& virtual_url);

// If |distilled_url| has a query "virtualURL" query params that is a URL,
// returns it. If not, return |distilled_url|
GURL VirtualURLForDistilledURL(const GURL& distilled_url);

// The file URL pointing to the local file to load to display |distilled_url|.
// If |resources_root_url| is not nullptr, it is set to a file URL to the
// directory conatining all the resources needed by |distilled_url|.
// |offline_path| is the root path to the directory containing offline files.
GURL FileURLForDistilledURL(const GURL& distilled_url,
                            const base::FilePath& offline_path,
                            GURL* resources_root_url);

// Returns whether the URL points to a chrome offline URL.
bool IsOfflineURL(const GURL& url);

// Strips scheme from the original URL of the offline page. This is meant to be
// used by UI.
// If |removed_chars| is non-NULL, it is set to the number of chars that have
// been removed at the begining of |online_url|.
base::string16 StripSchemeFromOnlineURL(const base::string16& online_url,
                                        size_t* removed_chars);

}  // namespace reading_list

#endif  // IOS_CHROME_BROWSER_READING_LIST_OFFLINE_URL_UTILS_H_
