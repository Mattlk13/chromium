// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/browser_prefs_android.h"

#include "blimp/client/public/blimp_client_context.h"
#include "chrome/browser/android/search_geolocation/search_geolocation_disclosure_tab_helper.h"
#include "chrome/browser/notifications/notification_platform_bridge_android.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace android {

void RegisterUserProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  NotificationPlatformBridgeAndroid::RegisterProfilePrefs(registry);
  SearchGeolocationDisclosureTabHelper::RegisterProfilePrefs(registry);
  blimp::client::BlimpClientContext::RegisterPrefs(registry);
}

}  // namespace android
