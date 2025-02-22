// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cryptauth/remote_device.h"

#include "base/base64.h"

namespace cryptauth {

RemoteDevice::RemoteDevice() : bluetooth_type(BLUETOOTH_CLASSIC) {}

RemoteDevice::RemoteDevice(const std::string& user_id,
                           const std::string& name,
                           const std::string& public_key,
                           BluetoothType bluetooth_type,
                           const std::string& bluetooth_address,
                           const std::string& persistent_symmetric_key,
                           std::string sign_in_challenge)
    : user_id(user_id),
      name(name),
      public_key(public_key),
      bluetooth_type(bluetooth_type),
      bluetooth_address(bluetooth_address),
      persistent_symmetric_key(persistent_symmetric_key),
      sign_in_challenge(sign_in_challenge) {}

RemoteDevice::RemoteDevice(const RemoteDevice& other) = default;

RemoteDevice::~RemoteDevice() {}

std::string RemoteDevice::GetDeviceId() const {
  std::string to_return;
  base::Base64Encode(public_key, &to_return);
  return to_return;
}

std::string RemoteDevice::GetTruncatedDeviceIdForLogs() const {
  return RemoteDevice::TruncateDeviceIdForLogs(GetDeviceId());
}

bool RemoteDevice::operator==(const RemoteDevice& other) const {
  return user_id == other.user_id
      && name == other.name
      && public_key == other.public_key
      && bluetooth_type == other.bluetooth_type
      && bluetooth_address == other.bluetooth_address
      && persistent_symmetric_key == other.persistent_symmetric_key
      && sign_in_challenge == other.sign_in_challenge;
}

// static
std::string RemoteDevice::TruncateDeviceIdForLogs(const std::string& full_id) {
  if (full_id.length() <= 10) {
    return full_id;
  }

  return full_id.substr(0, 5)
      + "..."
      + full_id.substr(full_id.length() - 5, full_id.length());
}

}  // namespace cryptauth
