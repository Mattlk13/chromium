// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_PROFILE_INFO_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_PROFILE_INFO_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "components/prefs/pref_change_registrar.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user_manager.h"
#else
#include "chrome/browser/profiles/profile_statistics_common.h"
#endif

class Profile;

namespace settings {

class ProfileInfoHandler : public SettingsPageUIHandler,
#if defined(OS_CHROMEOS)
                           public user_manager::UserManager::Observer,
#endif
                           public ProfileAttributesStorage::Observer {
 public:
  static const char kProfileInfoChangedEventName[];
  static const char kProfileManagesSupervisedUsersChangedEventName[];
  static const char kProfileStatsCountReadyEventName[];

  explicit ProfileInfoHandler(Profile* profile);
  ~ProfileInfoHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

#if defined(OS_CHROMEOS)
  // user_manager::UserManager::Observer implementation.
  void OnUserImageChanged(const user_manager::User& user) override;
#endif

  // ProfileAttributesStorage::Observer implementation.
  void OnProfileNameChanged(const base::FilePath& profile_path,
                            const base::string16& old_profile_name) override;
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoHandlerTest, GetProfileInfo);
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoHandlerTest, PushProfileInfo);
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoHandlerTest,
                           GetProfileManagesSupervisedUsers);
  FRIEND_TEST_ALL_PREFIXES(ProfileInfoHandlerTest,
                           PushProfileManagesSupervisedUsers);

  // Callbacks from the page.
  void HandleGetProfileInfo(const base::ListValue* args);
  void HandleGetProfileManagesSupervisedUsers(const base::ListValue* args);
  void PushProfileInfo();

#if !defined(OS_CHROMEOS)
  void HandleGetProfileStats(const base::ListValue* args);

  // Returns the sum of the counts of individual profile states. Returns 0 if
  // there exists a stat that was not successfully retrieved.
  void PushProfileStatsCount(profiles::ProfileCategoryStats stats);
#endif

  // Pushes whether the current profile manages supervised users to JavaScript.
  void PushProfileManagesSupervisedUsersStatus();

  // Returns true if this profile manages supervised users.
  bool IsProfileManagingSupervisedUsers() const;

  std::unique_ptr<base::DictionaryValue> GetAccountNameAndIcon() const;

  // Weak pointer.
  Profile* profile_;

#if defined(OS_CHROMEOS)
  ScopedObserver<user_manager::UserManager, ProfileInfoHandler>
      user_manager_observer_;
#endif

  ScopedObserver<ProfileAttributesStorage, ProfileInfoHandler>
      profile_observer_;

  // Used to listen for changes in the list of managed supervised users.
  PrefChangeRegistrar profile_pref_registrar_;

  DISALLOW_COPY_AND_ASSIGN(ProfileInfoHandler);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_PROFILE_INFO_HANDLER_H_
