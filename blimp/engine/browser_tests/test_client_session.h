// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_ENGINE_BROWSER_TESTS_TEST_CLIENT_SESSION_H_
#define BLIMP_ENGINE_BROWSER_TESTS_TEST_CLIENT_SESSION_H_

#include "blimp/engine/browser_tests/blimp_client_session.h"

namespace blimp {
namespace client {

class TestClientSession : public client::BlimpClientSession {
 public:
  TestClientSession();
  ~TestClientSession() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestClientSession);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_ENGINE_BROWSER_TESTS_TEST_CLIENT_SESSION_H_
