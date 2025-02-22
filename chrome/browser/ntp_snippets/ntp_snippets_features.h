// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_SNIPPETS_NTP_SNIPPETS_FEATURES_H_
#define CHROME_BROWSER_NTP_SNIPPETS_NTP_SNIPPETS_FEATURES_H_

#include "base/feature_list.h"

// Enables and configures notifications for content suggestions.
extern const base::Feature kContentSuggestionsNotificationsFeature;

// "false": use server signals to decide whether to send a notification
// "true": always send a notification when we receive ARTICLES suggestions
extern const char kContentSuggestionsNotificationsAlwaysNotifyParam[];

#endif  // CHROME_BROWSER_NTP_SNIPPETS_NTP_SNIPPETS_FEATURES_H_
