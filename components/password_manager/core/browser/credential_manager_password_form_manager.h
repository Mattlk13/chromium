// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PASSWORD_FORM_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PASSWORD_FORM_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/password_form_manager.h"

namespace autofill {
struct PasswordForm;
}

namespace password_manager {

class PasswordManagerClient;
class PasswordManagerDriver;

// A delegate that is notified when CredentialManagerPasswordFormManager
// finishes working with password forms.
class CredentialManagerPasswordFormManagerDelegate {
 public:
  // Called when a password form has been provisionally saved.
  virtual void OnProvisionalSaveComplete() = 0;
};

// A PasswordFormManager built to handle PasswordForm objects synthesized
// by the Credential Manager API.
class CredentialManagerPasswordFormManager : public PasswordFormManager {
 public:
  // Given a |client| and an |observed_form|, kick off the process of fetching
  // matching logins from the password store; if |observed_form| doesn't map to
  // a blacklisted origin, provisionally save |saved_form|. Once saved, let the
  // delegate know that it's safe to poke at the UI. |form_fetcher| is passed
  // to PasswordFormManager. |form_saver| can be null, in which case it is
  // created automatically.
  //
  // This class does not take ownership of |delegate|.
  CredentialManagerPasswordFormManager(
      PasswordManagerClient* client,
      base::WeakPtr<PasswordManagerDriver> driver,
      const autofill::PasswordForm& observed_form,
      std::unique_ptr<autofill::PasswordForm> saved_form,
      CredentialManagerPasswordFormManagerDelegate* delegate,
      std::unique_ptr<FormSaver> form_saver,
      FormFetcher* form_fetcher);
  ~CredentialManagerPasswordFormManager() override;

  void ProcessMatches(
      const std::vector<const autofill::PasswordForm*>& non_federated,
      size_t filtered_count) override;

 private:
  // Calls OnProvisionalSaveComplete on |delegate_|.
  void NotifyDelegate();

  CredentialManagerPasswordFormManagerDelegate* delegate_;
  std::unique_ptr<autofill::PasswordForm> saved_form_;

  base::WeakPtrFactory<CredentialManagerPasswordFormManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CredentialManagerPasswordFormManager);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CREDENTIAL_MANAGER_PASSWORD_FORM_MANAGER_H_
