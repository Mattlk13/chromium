// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_H
#define COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_H

#include <string>
#include <vector>

namespace cryptauth {

struct RemoteDevice {
 public:
  enum BluetoothType { BLUETOOTH_CLASSIC, BLUETOOTH_LE };

  std::string user_id;
  std::string name;
  std::string public_key;
  BluetoothType bluetooth_type;
  std::string bluetooth_address;
  std::string persistent_symmetric_key;
  std::string sign_in_challenge;

  RemoteDevice();
  RemoteDevice(const std::string& user_id,
               const std::string& name,
               const std::string& public_key,
               BluetoothType bluetooth_type,
               const std::string& bluetooth_address,
               const std::string& persistent_symmetric_key,
               std::string sign_in_challenge);
  RemoteDevice(const RemoteDevice& other);
  ~RemoteDevice();

  // Returns a unique ID for the device.
  std::string GetDeviceId() const;

  // Returns a shortened device ID for the purpose of concise logging (device
  // IDs are often so long that logs are difficult to read). Note that this
  // ID is not guaranteed to be unique, so it should only be used for log.
  std::string GetTruncatedDeviceIdForLogs() const;

  bool operator==(const RemoteDevice& other) const;

  // Static method for truncated device ID for logs.
  static std::string TruncateDeviceIdForLogs(const std::string& full_id);
};

typedef std::vector<RemoteDevice> RemoteDeviceList;

}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_REMOTE_DEVICE_H
