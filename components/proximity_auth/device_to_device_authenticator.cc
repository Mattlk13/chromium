// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proximity_auth/device_to_device_authenticator.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/cryptauth/connection.h"
#include "components/cryptauth/secure_message_delegate.h"
#include "components/cryptauth/wire_message.h"
#include "components/proximity_auth/device_to_device_initiator_operations.h"
#include "components/proximity_auth/device_to_device_secure_context.h"
#include "components/proximity_auth/logging/logging.h"
#include "components/proximity_auth/secure_context.h"

namespace proximity_auth {

namespace {

// The time to wait in seconds for the remote device to send its
// [Responder Auth] message. If we do not get the message in this time, then
// authentication will fail.
const int kResponderAuthTimeoutSeconds = 5;

// The prefix of the permit id sent to the remote device. The permit id
// is used by the remote device to find the credentials of the local device.
const char kPermitIdPrefix[] = "permit://google.com/easyunlock/v1/";

}  // namespace

DeviceToDeviceAuthenticator::DeviceToDeviceAuthenticator(
    cryptauth::Connection* connection,
    const std::string& account_id,
    std::unique_ptr<cryptauth::SecureMessageDelegate> secure_message_delegate)
    : connection_(connection),
      account_id_(account_id),
      secure_message_delegate_(std::move(secure_message_delegate)),
      state_(State::NOT_STARTED),
      weak_ptr_factory_(this) {
  DCHECK(connection_);
}

DeviceToDeviceAuthenticator::~DeviceToDeviceAuthenticator() {
  connection_->RemoveObserver(this);
}

void DeviceToDeviceAuthenticator::Authenticate(
    const AuthenticationCallback& callback) {
  if (state_ != State::NOT_STARTED) {
    PA_LOG(ERROR)
        << "Authenticator was already used. Do not reuse this instance!";
    callback.Run(Result::FAILURE, nullptr);
    return;
  }

  callback_ = callback;
  if (!connection_->IsConnected()) {
    Fail("Not connected to remote device", Result::DISCONNECTED);
    return;
  }

  connection_->AddObserver(this);

  // Generate a key-pair for this individual session.
  state_ = State::GENERATING_SESSION_KEYS;
  secure_message_delegate_->GenerateKeyPair(
      base::Bind(&DeviceToDeviceAuthenticator::OnKeyPairGenerated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DeviceToDeviceAuthenticator::OnKeyPairGenerated(
    const std::string& public_key,
    const std::string& private_key) {
  DCHECK(state_ == State::GENERATING_SESSION_KEYS);
  if (public_key.empty() || private_key.empty()) {
    Fail("Failed to generate session keys");
    return;
  }
  local_session_private_key_ = private_key;

  // Create the [Hello] message to send to the remote device.
  state_ = State::SENDING_HELLO;
  DeviceToDeviceInitiatorOperations::CreateHelloMessage(
      public_key, connection_->remote_device().persistent_symmetric_key,
      secure_message_delegate_.get(),
      base::Bind(&DeviceToDeviceAuthenticator::OnHelloMessageCreated,
                 weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<base::Timer> DeviceToDeviceAuthenticator::CreateTimer() {
  return base::MakeUnique<base::OneShotTimer>();
}

void DeviceToDeviceAuthenticator::OnHelloMessageCreated(
    const std::string& message) {
  DCHECK(state_ == State::SENDING_HELLO);
  if (message.empty()) {
    Fail("Failed to create [Hello]");
    return;
  }

  // Add a timeout for receiving the [Responder Auth] message as a guard.
  timer_ = CreateTimer();
  timer_->Start(
      FROM_HERE, base::TimeDelta::FromSeconds(kResponderAuthTimeoutSeconds),
      base::Bind(&DeviceToDeviceAuthenticator::OnResponderAuthTimedOut,
                 weak_ptr_factory_.GetWeakPtr()));

  // Send the [Hello] message to the remote device.
  state_ = State::SENT_HELLO;
  hello_message_ = message;
  std::string permit_id = kPermitIdPrefix + account_id_;
  connection_->SendMessage(
      base::MakeUnique<cryptauth::WireMessage>(hello_message_, permit_id));
}

void DeviceToDeviceAuthenticator::OnResponderAuthTimedOut() {
  DCHECK(state_ == State::SENT_HELLO);
  Fail("Timed out waiting for [Responder Auth]");
}

void DeviceToDeviceAuthenticator::OnResponderAuthValidated(
    bool validated,
    const std::string& session_symmetric_key) {
  if (!validated) {
    Fail("Unable to validated [Responder Auth]");
    return;
  }

  PA_LOG(INFO) << "Successfully validated [Responder Auth]! "
               << "Sending [Initiator Auth]...";
  state_ = State::VALIDATED_RESPONDER_AUTH;
  session_symmetric_key_ = session_symmetric_key;

  // Create the [Initiator Auth] message to send to the remote device.
  DeviceToDeviceInitiatorOperations::CreateInitiatorAuthMessage(
      session_symmetric_key,
      connection_->remote_device().persistent_symmetric_key,
      responder_auth_message_, secure_message_delegate_.get(),
      base::Bind(&DeviceToDeviceAuthenticator::OnInitiatorAuthCreated,
                 weak_ptr_factory_.GetWeakPtr()));
}

void DeviceToDeviceAuthenticator::OnInitiatorAuthCreated(
    const std::string& message) {
  DCHECK(state_ == State::VALIDATED_RESPONDER_AUTH);
  if (message.empty()) {
    Fail("Failed to create [Initiator Auth]");
    return;
  }

  state_ = State::SENT_INITIATOR_AUTH;
  connection_->SendMessage(base::MakeUnique<cryptauth::WireMessage>(message));
}

void DeviceToDeviceAuthenticator::Fail(const std::string& error_message) {
  Fail(error_message, Result::FAILURE);
}

void DeviceToDeviceAuthenticator::Fail(const std::string& error_message,
                                       Result result) {
  DCHECK(result != Result::SUCCESS);
  PA_LOG(WARNING) << "Authentication failed: " << error_message;
  state_ = State::AUTHENTICATION_FAILURE;
  weak_ptr_factory_.InvalidateWeakPtrs();
  connection_->RemoveObserver(this);
  timer_.reset();
  callback_.Run(result, nullptr);
}

void DeviceToDeviceAuthenticator::Succeed() {
  DCHECK(state_ == State::SENT_INITIATOR_AUTH);
  DCHECK(!session_symmetric_key_.empty());
  PA_LOG(INFO) << "Authentication succeeded!";

  state_ = State::AUTHENTICATION_SUCCESS;
  connection_->RemoveObserver(this);
  callback_.Run(
      Result::SUCCESS,
      base::MakeUnique<DeviceToDeviceSecureContext>(
          std::move(secure_message_delegate_), session_symmetric_key_,
          responder_auth_message_, SecureContext::PROTOCOL_VERSION_THREE_ONE));
}

void DeviceToDeviceAuthenticator::OnConnectionStatusChanged(
    cryptauth::Connection* connection,
    cryptauth::Connection::Status old_status,
    cryptauth::Connection::Status new_status) {
  // We do not expect the connection to drop during authentication.
  if (new_status == cryptauth::Connection::DISCONNECTED) {
    Fail("Disconnected while authentication is in progress",
         Result::DISCONNECTED);
  }
}

void DeviceToDeviceAuthenticator::OnMessageReceived(
    const cryptauth::Connection& connection,
    const cryptauth::WireMessage& message) {
  if (state_ == State::SENT_HELLO) {
    PA_LOG(INFO) << "Received [Responder Auth] message, payload_size="
                 << message.payload().size();
    state_ = State::RECEIVED_RESPONDER_AUTH;
    timer_.reset();
    responder_auth_message_ = message.payload();

    // Attempt to validate the [Responder Auth] message received from the remote
    // device.
    std::string responder_public_key = connection.remote_device().public_key;
    DeviceToDeviceInitiatorOperations::ValidateResponderAuthMessage(
        responder_auth_message_, responder_public_key,
        connection_->remote_device().persistent_symmetric_key,
        local_session_private_key_, hello_message_,
        secure_message_delegate_.get(),
        base::Bind(&DeviceToDeviceAuthenticator::OnResponderAuthValidated,
                   weak_ptr_factory_.GetWeakPtr()));
  } else {
    Fail("Unexpected message received");
  }
}

void DeviceToDeviceAuthenticator::OnSendCompleted(
    const cryptauth::Connection& connection,
    const cryptauth::WireMessage& message,
    bool success) {
  if (state_ == State::SENT_INITIATOR_AUTH) {
    if (success)
      Succeed();
    else
      Fail("Failed to send [Initiator Auth]");
  } else if (!success && state_ == State::SENT_HELLO) {
    DCHECK(message.payload() == hello_message_);
    Fail("Failed to send [Hello]");
  }
}

}  // namespace proximity_auth
