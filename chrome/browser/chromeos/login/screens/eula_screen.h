// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_EULA_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_EULA_SCREEN_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/eula_model.h"
#include "chromeos/tpm/tpm_password_fetcher.h"
#include "components/login/screens/screen_context.h"
#include "url/gurl.h"

namespace chromeos {

// Representation independent class that controls OOBE screen showing EULA
// to users.
class EulaScreen : public EulaModel, public TpmPasswordFetcherDelegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Whether usage statistics reporting is enabled on EULA screen.
    virtual void SetUsageStatisticsReporting(bool val) = 0;
    virtual bool GetUsageStatisticsReporting() const = 0;
  };

  EulaScreen(BaseScreenDelegate* base_screen_delegate,
             Delegate* delegate,
             EulaView* view);
  ~EulaScreen() override;

  // EulaModel implementation:
  void Show() override;
  void Hide() override;
  GURL GetOemEulaUrl() const override;
  void InitiatePasswordFetch() override;
  bool IsUsageStatsEnabled() const override;
  void OnViewDestroyed(EulaView* view) override;
  void OnUserAction(const std::string& action_id) override;
  void OnContextKeyUpdated(const ::login::ScreenContext::KeyType& key) override;

  // TpmPasswordFetcherDelegate implementation:
  void OnPasswordFetched(const std::string& tpm_password) override;

 private:
  // URL of the OEM EULA page (on disk).
  GURL oem_eula_page_;

  // TPM password local storage. By convention, we clear the password
  // from TPM as soon as we read it. We store it here locally until
  // EULA screen is closed.
  // TODO(glotov): Sanitize memory used to store password when
  // it's destroyed.
  std::string tpm_password_;

  Delegate* delegate_;

  EulaView* view_;

  TpmPasswordFetcher password_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(EulaScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_EULA_SCREEN_H_
