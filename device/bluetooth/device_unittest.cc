// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/device.h"

#include <memory>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_gatt_connection.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;

namespace bluetooth {

using NiceMockBluetoothAdapter =
    testing::NiceMock<device::MockBluetoothAdapter>;
using NiceMockBluetoothDevice = testing::NiceMock<device::MockBluetoothDevice>;
using NiceMockBluetoothGattService =
    testing::NiceMock<device::MockBluetoothGattService>;
using NiceMockBluetoothGattConnection =
    testing::NiceMock<device::MockBluetoothGattConnection>;

namespace {
const char kTestLeDeviceAddress0[] = "11:22:33:44:55:66";
const char kTestLeDeviceName0[] = "Test LE Device 0";

const char kTestServiceId0[] = "service_id0";
const char kTestServiceUuid0[] = "1234";

const char kTestServiceId1[] = "service_id1";
const char kTestServiceUuid1[] = "5678";

class BluetoothInterfaceDeviceTest : public testing::Test {
 public:
  enum class Call { EXPECTED, NOT_EXPECTED };

  BluetoothInterfaceDeviceTest()
      : adapter_(new NiceMockBluetoothAdapter),
        device_(adapter_.get(),
                0,
                kTestLeDeviceName0,
                kTestLeDeviceAddress0,
                false,
                true),
        weak_factory_(this) {
    ON_CALL(*adapter_, GetDevice(kTestLeDeviceAddress0))
        .WillByDefault(Return(&device_));

    auto service1 = base::MakeUnique<NiceMockBluetoothGattService>(
        &device_, kTestServiceId0, device::BluetoothUUID(kTestServiceUuid0),
        true /* is_primary */, false /* is_local */);
    auto service2 = base::MakeUnique<NiceMockBluetoothGattService>(
        &device_, kTestServiceId1, device::BluetoothUUID(kTestServiceUuid1),
        true /* is_primary */, false /* is_local */);

    device_.AddMockService(std::move(service1));
    device_.AddMockService(std::move(service2));

    EXPECT_CALL(device_, GetGattServices())
        .WillRepeatedly(
            Invoke(&device_, &device::MockBluetoothDevice::GetMockServices));

    auto connection = base::MakeUnique<NiceMockBluetoothGattConnection>(
        adapter_, device_.GetAddress());

    Device::Create(adapter_, std::move(connection), mojo::MakeRequest(&proxy_));

    proxy_.set_connection_error_handler(
        base::Bind(&BluetoothInterfaceDeviceTest::OnConnectionError,
                   weak_factory_.GetWeakPtr()));
  }

  void TearDown() override {
    EXPECT_EQ(expected_success_callback_calls_, actual_success_callback_calls_);
    EXPECT_EQ(message_pipe_closed_, expect_device_service_deleted_);
    proxy_.reset();
  }

 protected:
  void OnConnectionError() { message_pipe_closed_ = true; }

  void SimulateGattServicesDiscovered() {
    for (auto& observer : adapter_->GetObservers())
      observer.GattServicesDiscovered(adapter_.get(), &device_);
  }

  void SimulateDeviceChanged() {
    for (auto& observer : adapter_->GetObservers())
      observer.DeviceChanged(adapter_.get(), &device_);
  }

  void GetServicesCheckForPrecedingCalls(
      Call expected,
      size_t expected_service_count,
      int num_of_preceding_calls,
      std::vector<mojom::ServiceInfoPtr> services) {
    EXPECT_EQ(num_of_preceding_calls, callback_count_);
    ++callback_count_;

    if (expected == Call::EXPECTED)
      ++actual_success_callback_calls_;

    EXPECT_EQ(expected_service_count, services.size());
  }

  Device::GetServicesCallback GetGetServicesCheckForPrecedingCalls(
      Call expected,
      int num_of_preceding_calls) {
    if (expected == Call::EXPECTED)
      ++expected_success_callback_calls_;

    return base::Bind(
        &BluetoothInterfaceDeviceTest::GetServicesCheckForPrecedingCalls,
        weak_factory_.GetWeakPtr(), expected, 2 /* expected_service_count */,
        num_of_preceding_calls);
  }

  scoped_refptr<NiceMockBluetoothAdapter> adapter_;
  NiceMockBluetoothDevice device_;
  base::MessageLoop message_loop_;
  mojom::DevicePtr proxy_;
  mojo::StrongBindingPtr<mojom::Device> binding_ptr_;

  bool message_pipe_closed_ = false;
  bool expect_device_service_deleted_ = false;
  int expected_success_callback_calls_ = 0;
  int actual_success_callback_calls_ = 0;
  int callback_count_ = 0;

  base::WeakPtrFactory<BluetoothInterfaceDeviceTest> weak_factory_;
};
}  // namespace

TEST_F(BluetoothInterfaceDeviceTest, GetServices) {
  EXPECT_CALL(device_, IsGattServicesDiscoveryComplete())
      .WillRepeatedly(Return(true));

  proxy_->GetServices(GetGetServicesCheckForPrecedingCalls(
      Call::EXPECTED, 0 /* num_of_preceding_calls */));

  base::RunLoop().RunUntilIdle();
}

TEST_F(BluetoothInterfaceDeviceTest, GetServicesNotDiscovered) {
  EXPECT_CALL(device_, IsGattServicesDiscoveryComplete())
      .WillOnce(Return(false))
      .WillOnce(Return(false))
      .WillRepeatedly(Return(true));

  // Client: Sends multiple requests for services.
  proxy_->GetServices(GetGetServicesCheckForPrecedingCalls(
      Call::EXPECTED, 0 /* num_of_preceding_calls */));
  proxy_->GetServices(GetGetServicesCheckForPrecedingCalls(
      Call::EXPECTED, 1 /* num_of_preceding_calls */));

  base::RunLoop().RunUntilIdle();

  SimulateGattServicesDiscovered();

  // No more GetServices calls will complete.
  SimulateGattServicesDiscovered();

  base::RunLoop().RunUntilIdle();

  // Client: Sends more requests which run immediately.
  proxy_->GetServices(GetGetServicesCheckForPrecedingCalls(
      Call::EXPECTED, 2 /* num_of_preceding_calls */));
  proxy_->GetServices(GetGetServicesCheckForPrecedingCalls(
      Call::EXPECTED, 3 /* num_of_preceding_calls */));

  base::RunLoop().RunUntilIdle();

  // No more GetServices calls will complete.
  SimulateGattServicesDiscovered();

  // Wait for message pipe to process error.
  base::RunLoop().RunUntilIdle();
}

TEST_F(BluetoothInterfaceDeviceTest,
       GetServicesLostConnectionWithPendingRequests) {
  EXPECT_CALL(device_, IsGattServicesDiscoveryComplete())
      .WillRepeatedly(Return(false));
  // Client: Sends multiple requests for services.
  proxy_->GetServices(GetGetServicesCheckForPrecedingCalls(
      Call::NOT_EXPECTED, 0 /* num_of_preceding_calls */));
  proxy_->GetServices(GetGetServicesCheckForPrecedingCalls(
      Call::NOT_EXPECTED, 1 /* num_of_preceding_calls */));
  EXPECT_EQ(0, callback_count_);

  // Simulate connection loss.
  device_.SetConnected(false);
  SimulateDeviceChanged();
  expect_device_service_deleted_ = true;

  // Wait for message pipe to process error.
  base::RunLoop().RunUntilIdle();
}

TEST_F(BluetoothInterfaceDeviceTest,
       GetServicesForcedDisconnectionWithPendingRequests) {
  EXPECT_CALL(device_, IsGattServicesDiscoveryComplete())
      .WillRepeatedly(Return(false));

  // Client: Sends multiple requests for services.
  proxy_->GetServices(GetGetServicesCheckForPrecedingCalls(
      Call::NOT_EXPECTED, 0 /* num_of_preceding_calls */));
  proxy_->GetServices(GetGetServicesCheckForPrecedingCalls(
      Call::NOT_EXPECTED, 1 /* num_of_preceding_calls */));
  EXPECT_EQ(0, callback_count_);

  // Simulate connection loss.
  proxy_->Disconnect();
  expect_device_service_deleted_ = true;

  // Wait for message pipe to process error.
  base::RunLoop().RunUntilIdle();
}

}  // namespace bluetooth
