// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/oobe_screen.h"

#include "base/logging.h"
#include "base/macros.h"

namespace chromeos {
namespace {

// These get mapped by the Screen enum ordinal values, so this has to be defined
// in the same order as the Screen enum.
const char* kScreenNames[] = {
    "hid-detection",                   // SCREEN_OOBE_HID_DETECTION
    "connect",                         // SCREEN_OOBE_NETWORK
    "eula",                            // SCREEN_OOBE_EULA
    "update",                          // SCREEN_OOBE_UPDATE
    "debugging",                       // SCREEN_OOBE_ENABLE_DEBUGGING
    "oauth-enrollment",                // SCREEN_OOBE_ENROLLMENT
    "reset",                           // SCREEN_OOBE_RESET
    "gaia-signin",                     // SCREEN_GAIA_SIGNIN
    "account-picker",                  // SCREEN_ACCOUNT_PICKER
    "autolaunch",                      // SCREEN_KIOSK_AUTOLAUNCH
    "kiosk-enable",                    // SCREEN_KIOSK_ENABLE
    "error-message",                   // SCREEN_ERROR_MESSAGE
    "user-image",                      // SCREEN_USER_IMAGE_PICKER
    "tpm-error-message",               // SCREEN_TPM_ERROR
    "password-changed",                // SCREEN_PASSWORD_CHANGED
    "supervised-user-creation",        // SCREEN_CREATE_SUPERVISED_USER_FLOW
    "terms-of-service",                // SCREEN_TERMS_OF_SERVICE
    "arc-tos",                         // SCREEN_ARC_TERMS_OF_SERVICE
    "wrong-hwid",                      // SCREEN_WRONG_HWID
    "auto-enrollment-check",           // SCREEN_AUTO_ENROLLMENT_CHECK
    "app-launch-splash",               // SCREEN_APP_LAUNCH_SPLASH
    "confirm-password",                // SCREEN_CONFIRM_PASSWORD
    "fatal-error",                     // SCREEN_FATAL_ERROR
    "controller-pairing",              // SCREEN_OOBE_CONTROLLER_PAIRING
    "host-pairing",                    // SCREEN_OOBE_HOST_PAIRING
    "device-disabled",                 // SCREEN_DEVICE_DISABLED
    "unrecoverable-cryptohome-error",  // SCREEN_UNRECOVERABLE_CRYPTOHOME_ERROR
    "userBoard",                       // SCREEN_USER_SELECTION
    "login",                           // SCREEN_SPECIAL_LOGIN
    "oobe",                            // SCREEN_SPECIAL_OOBE
    "test:nowindow",                   // SCREEN_TEST_NO_WINDOW
    "unknown",                         // SCREEN_UNKNOWN
};

static_assert(static_cast<size_t>(OobeScreen::SCREEN_UNKNOWN) ==
                  arraysize(kScreenNames) - 1,
              "Missing element in OobeScreen or kScreenNames");

}  // namespace

std::string GetOobeScreenName(OobeScreen screen) {
  DCHECK(screen <= OobeScreen::SCREEN_UNKNOWN);
  return kScreenNames[static_cast<size_t>(screen)];
}

OobeScreen GetOobeScreenFromName(const std::string& name) {
  for (size_t i = 0; i < arraysize(kScreenNames); ++i) {
    if (name == kScreenNames[i])
      return static_cast<OobeScreen>(i);
  }

  return OobeScreen::SCREEN_UNKNOWN;
}

}  // namespace chromeos
