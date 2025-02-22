// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_CHROME_SESSION_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_CHROME_SESSION_MANAGER_H_

#include "base/macros.h"
#include "components/session_manager/core/session_manager.h"

namespace base {
class CommandLine;
}

class Profile;

namespace chromeos {

class IdleDetector;

class ChromeSessionManager : public session_manager::SessionManager {
 public:
  ChromeSessionManager();
  ~ChromeSessionManager() override;

  // Initialize session manager on browser starts up. Runs different code
  // path based on command line flags and policy. Possible scenarios include:
  //   - Launches pre-session UI such as  out-of-box or login;
  //   - Launches the auto launched kiosk app;
  //   - Resumes user sessions on crash-and-restart;
  //   - Starts a stub login session for dev or test;
  void Initialize(const base::CommandLine& parsed_command_line,
                  Profile* profile,
                  bool is_running_test);

  // session_manager::SessionManager:
  void SessionStarted() override;
  void NotifyUserLoggedIn(const AccountId& user_account_id,
                          const std::string& user_id_hash,
                          bool browser_restart) override;

 private:
  // Used to preload the lock screen when the user is inactive.
  std::unique_ptr<IdleDetector> idle_detector_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSessionManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_CHROME_SESSION_MANAGER_H_
