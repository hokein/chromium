// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/encryptor/encryptor_switches.h"

namespace encryptor {
namespace switches {

#if defined(OS_MACOSX)

const char kUseMockKeychain[] = "use-mock-keychain";

#endif  // OS_MACOSX

}  // namespace switches
}  // namespace encryptor
