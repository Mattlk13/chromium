// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_REMOTE_NTP_SNIPPETS_REQUEST_PARAMS_H_
#define COMPONENTS_NTP_SNIPPETS_REMOTE_NTP_SNIPPETS_REQUEST_PARAMS_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/optional.h"
#include "base/values.h"
#include "components/ntp_snippets/category.h"

namespace ntp_snippets {

// Enumeration listing all possible variants of dealing with personalization.
enum class Personalization { kPersonal, kNonPersonal, kBoth };

// Contains all parameters for fetching NTPSnippets.
struct NTPSnippetsRequestParams {
  NTPSnippetsRequestParams();
  NTPSnippetsRequestParams(const NTPSnippetsRequestParams&);
  ~NTPSnippetsRequestParams();

  // BCP 47 language code specifying the user's UI language.
  std::string language_code;

  // A set of suggestion IDs that should not be returned again.
  std::set<std::string> excluded_ids;

  // Maximum number of snippets to fetch.
  int count_to_fetch = 0;

  // Whether this is an interactive request, i.e. triggered by an explicit
  // user action. Typically, non-interactive requests are subject to a daily
  // quota.
  bool interactive_request = false;

  // If set, only return results for this category.
  base::Optional<Category> exclusive_category;
};

// Callbacks for JSON parsing to allow injecting platform-dependent code.
using SuccessCallback =
    base::Callback<void(std::unique_ptr<base::Value> result)>;
using ErrorCallback = base::Callback<void(const std::string& error)>;
using ParseJSONCallback =
    base::Callback<void(const std::string& raw_json_string,
                        const SuccessCallback& success_callback,
                        const ErrorCallback& error_callback)>;

}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_REMOTE_NTP_SNIPPETS_REQUEST_PARAMS_H_
