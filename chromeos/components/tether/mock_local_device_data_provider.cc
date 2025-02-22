// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/mock_local_device_data_provider.h"

#include "components/cryptauth/cryptauth_device_manager.h"
#include "components/cryptauth/cryptauth_enrollment_manager.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"

namespace chromeos {

namespace tether {

MockLocalDeviceDataProvider::MockLocalDeviceDataProvider()
    : LocalDeviceDataProvider(nullptr, nullptr) {}

MockLocalDeviceDataProvider::~MockLocalDeviceDataProvider() {}

void MockLocalDeviceDataProvider::SetPublicKey(
    std::unique_ptr<std::string> public_key) {
  if (public_key) {
    public_key_ = std::move(public_key);
  } else {
    public_key_.reset();
  }
}

void MockLocalDeviceDataProvider::SetBeaconSeeds(
    std::unique_ptr<std::vector<cryptauth::BeaconSeed>> beacon_seeds) {
  if (beacon_seeds) {
    beacon_seeds_ = std::move(beacon_seeds);
  } else {
    beacon_seeds_.reset();
  }
}

bool MockLocalDeviceDataProvider::GetLocalDeviceData(
    std::string* public_key_out,
    std::vector<cryptauth::BeaconSeed>* beacon_seeds_out) const {
  if (public_key_ && beacon_seeds_) {
    if (public_key_out) {
      *public_key_out = *public_key_;
    }
    if (beacon_seeds_out) {
      *beacon_seeds_out = *beacon_seeds_;
    }
    return true;
  }

  return false;
}

}  // namespace tether

}  // namespace cryptauth
