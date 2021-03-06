// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_GENERATION_MANAGER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_GENERATION_MANAGER_H_

#include <vector>

#include "base/basictypes.h"

class PasswordManager;
class PasswordManagerClient;
class PasswordManagerDriver;

namespace autofill {
class FormStructure;
}

// Per-tab manager for password generation. Will enable this feature only if
//
// -  Password manager is enabled
// -  Password sync is enabled
//
// NOTE: At the moment, the creation of the renderer PasswordGenerationManager
// is controlled by a switch (--enable-password-generation) so this feature will
// not be enabled regardless of the above criteria without the switch being
// present.
//
// This class is used to determine what forms we should offer to generate
// passwords for and manages the popup which is created if the user chooses to
// generate a password.
class PasswordGenerationManager {
 public:
  explicit PasswordGenerationManager(PasswordManagerClient* client);
  virtual ~PasswordGenerationManager();

  // Detect account creation forms from forms with autofill type annotated.
  // Will send a message to the renderer if we find a correctly annotated form
  // and the feature is enabled.
  void DetectAccountCreationForms(
      const std::vector<autofill::FormStructure*>& forms);

 private:
  friend class PasswordGenerationManagerTest;

  // Determines current state of password generation
  bool IsGenerationEnabled() const;

  // The PasswordManagerClient instance associated with this instance. Must
  // outlive this instance.
  PasswordManagerClient* client_;

  // The PasswordManagerDriver instance associated with this instance. Must
  // outlive this instance.
  PasswordManagerDriver* driver_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationManager);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_GENERATION_MANAGER_H_
