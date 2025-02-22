// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/page_info/website_settings_popup_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/stl_util.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/website_settings/website_settings.h"
#include "chrome/browser/ui/website_settings/website_settings_ui.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "jni/WebsiteSettingsPopup_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;

// static
static jlong Init(JNIEnv* env,
                  const JavaParamRef<jclass>& clazz,
                  const JavaParamRef<jobject>& obj,
                  const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);

  return reinterpret_cast<intptr_t>(
      new WebsiteSettingsPopupAndroid(env, obj, web_contents));
}

WebsiteSettingsPopupAndroid::WebsiteSettingsPopupAndroid(
    JNIEnv* env,
    jobject java_website_settings_pop,
    content::WebContents* web_contents) {
  // Important to use GetVisibleEntry to match what's showing in the omnibox.
  content::NavigationEntry* nav_entry =
      web_contents->GetController().GetVisibleEntry();
  if (nav_entry == NULL)
    return;

  url_ = nav_entry->GetURL();

  popup_jobject_.Reset(env, java_website_settings_pop);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  DCHECK(helper);
  security_state::SecurityInfo security_info;
  helper->GetSecurityInfo(&security_info);

  presenter_.reset(new WebsiteSettings(
      this, Profile::FromBrowserContext(web_contents->GetBrowserContext()),
      TabSpecificContentSettings::FromWebContents(web_contents), web_contents,
      nav_entry->GetURL(), security_info));
}

WebsiteSettingsPopupAndroid::~WebsiteSettingsPopupAndroid() {}

void WebsiteSettingsPopupAndroid::Destroy(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  delete this;
}

void WebsiteSettingsPopupAndroid::RecordWebsiteSettingsAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint action) {
  presenter_->RecordWebsiteSettingsAction(
      static_cast<WebsiteSettings::WebsiteSettingsAction>(action));
}

void WebsiteSettingsPopupAndroid::SetIdentityInfo(
    const IdentityInfo& identity_info) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::unique_ptr<WebsiteSettingsUI::SecurityDescription> security_description =
      identity_info.GetSecurityDescription();

  Java_WebsiteSettingsPopup_setSecurityDescription(
      env, popup_jobject_,
      ConvertUTF16ToJavaString(env, security_description->summary),
      ConvertUTF16ToJavaString(env, security_description->details));
}

void WebsiteSettingsPopupAndroid::SetCookieInfo(
    const CookieInfoList& cookie_info_list) {
  NOTIMPLEMENTED();
}

void WebsiteSettingsPopupAndroid::SetPermissionInfo(
    const PermissionInfoList& permission_info_list,
    ChosenObjectInfoList chosen_object_info_list) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // On Android, we only want to display a subset of the available options in a
  // particular order, but only if their value is different from the default.
  std::vector<ContentSettingsType> permissions_to_display;
  permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_GEOLOCATION);
  permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
  permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
  permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_IMAGES);
  permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_JAVASCRIPT);
  permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_POPUPS);
  permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_AUTOPLAY);

  std::map<ContentSettingsType, ContentSetting>
      user_specified_settings_to_display;

  for (const auto& permission : permission_info_list) {
    if (base::ContainsValue(permissions_to_display, permission.type) &&
        permission.setting != CONTENT_SETTING_DEFAULT) {
      user_specified_settings_to_display[permission.type] = permission.setting;
    }
  }

  for (const auto& permission : permissions_to_display) {
    if (base::ContainsKey(user_specified_settings_to_display, permission)) {
      base::string16 setting_title =
          WebsiteSettingsUI::PermissionTypeToUIString(permission);

      Java_WebsiteSettingsPopup_addPermissionSection(
          env, popup_jobject_, ConvertUTF16ToJavaString(env, setting_title),
          static_cast<jint>(permission),
          static_cast<jint>(user_specified_settings_to_display[permission]));
    }
  }

  for (const auto& chosen_object : chosen_object_info_list) {
    base::string16 object_title =
        WebsiteSettingsUI::ChosenObjectToUIString(*chosen_object);

    Java_WebsiteSettingsPopup_addPermissionSection(
        env, popup_jobject_, ConvertUTF16ToJavaString(env, object_title),
        static_cast<jint>(chosen_object->ui_info.content_settings_type),
        static_cast<jint>(CONTENT_SETTING_ALLOW));
  }

  Java_WebsiteSettingsPopup_updatePermissionDisplay(env, popup_jobject_);
}

// static
bool WebsiteSettingsPopupAndroid::RegisterWebsiteSettingsPopupAndroid(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}
