// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/hid_detection_screen.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/screens/base_screen_delegate.h"
#include "chrome/browser/chromeos/login/screens/hid_detection_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/grit/generated_resources.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Possible ui-states for device-blocks.
const char kSearchingState[] = "searching";
const char kUSBState[] = "usb";
const char kConnectedState[] = "connected";
const char kBTPairedState[] = "paired";
const char kBTPairingState[] = "pairing";

// Standard length of pincode for pairing BT keyboards.
const int kPincodeLength = 6;

bool DeviceIsPointing(device::BluetoothDeviceType device_type) {
  return device_type == device::BluetoothDeviceType::MOUSE ||
         device_type == device::BluetoothDeviceType::KEYBOARD_MOUSE_COMBO ||
         device_type == device::BluetoothDeviceType::TABLET;
}

bool DeviceIsPointing(const device::InputServiceLinux::InputDeviceInfo& info) {
  return info.is_mouse || info.is_touchpad || info.is_touchscreen ||
         info.is_tablet;
}

bool DeviceIsKeyboard(device::BluetoothDeviceType device_type) {
  return device_type == device::BluetoothDeviceType::KEYBOARD ||
         device_type == device::BluetoothDeviceType::KEYBOARD_MOUSE_COMBO;
}

}  // namespace

namespace chromeos {

HIDDetectionScreen::HIDDetectionScreen(BaseScreenDelegate* base_screen_delegate,
                                       HIDDetectionView* view)
    : HIDDetectionModel(base_screen_delegate),
      view_(view),
      weak_ptr_factory_(this) {
  DCHECK(view_);
  if (view_)
    view_->Bind(*this);
}

HIDDetectionScreen::~HIDDetectionScreen() {
  adapter_initially_powered_.reset();
  input_service_proxy_.RemoveObserver(this);
  if (view_)
    view_->Unbind();
  if (discovery_session_.get())
    discovery_session_->Stop(base::Bind(&base::DoNothing),
                             base::Bind(&base::DoNothing));
  if (adapter_.get())
    adapter_->RemoveObserver(this);
}

void HIDDetectionScreen::Show() {
  showing_ = true;
  GetContextEditor().SetBoolean(kContextKeyNumKeysEnteredExpected, false);
  SendPointingDeviceNotification();
  SendKeyboardDeviceNotification();

  input_service_proxy_.AddObserver(this);
  UpdateDevices();

  if (view_)
    view_->Show();
}

void HIDDetectionScreen::Hide() {
  showing_ = false;
  input_service_proxy_.RemoveObserver(this);
  if (discovery_session_.get())
    discovery_session_->Stop(base::Bind(&base::DoNothing),
                             base::Bind(&base::DoNothing));
  if (view_)
    view_->Hide();
}

void HIDDetectionScreen::Initialize(::login::ScreenContext* context) {
  HIDDetectionModel::Initialize(context);

  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&HIDDetectionScreen::InitializeAdapter,
                 weak_ptr_factory_.GetWeakPtr()));
}

void HIDDetectionScreen::OnContinueButtonClicked() {

  ContinueScenarioType scenario_type;
  if (!pointing_device_id_.empty() && !keyboard_device_id_.empty())
    scenario_type = All_DEVICES_DETECTED;
  else if (pointing_device_id_.empty())
    scenario_type = KEYBOARD_DEVICE_ONLY_DETECTED;
  else
    scenario_type = POINTING_DEVICE_ONLY_DETECTED;

  UMA_HISTOGRAM_ENUMERATION(
      "HIDDetection.OOBEDevicesDetectedOnContinuePressed",
      scenario_type,
      CONTINUE_SCENARIO_TYPE_SIZE);

  // Switch off BT adapter if it was off before the screen and no BT device
  // connected.
  const bool adapter_is_powered =
      adapter_.get() && adapter_->IsPresent() && adapter_->IsPowered();
  const bool need_switching_off =
      adapter_initially_powered_ && !(*adapter_initially_powered_);
  if (adapter_is_powered && need_switching_off) {
    input_service_proxy_.GetDevices(
        base::Bind(&HIDDetectionScreen::OnGetInputDevicesForPowerOff,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  Finish(BaseScreenDelegate::HID_DETECTION_COMPLETED);
}

void HIDDetectionScreen::CheckIsScreenRequired(
      const base::Callback<void(bool)>& on_check_done) {
  input_service_proxy_.GetDevices(
      base::Bind(&HIDDetectionScreen::OnGetInputDevicesListForCheck,
                 weak_ptr_factory_.GetWeakPtr(),
                 on_check_done));
}

void HIDDetectionScreen::OnViewDestroyed(HIDDetectionView* view) {
  if (view_ == view)
    view_ = NULL;
}

void HIDDetectionScreen::RequestPinCode(device::BluetoothDevice* device) {
  VLOG(1) << "RequestPinCode id = " << device->GetDeviceID()
          << " name = " << device->GetNameForDisplay();
  device->CancelPairing();
}

void HIDDetectionScreen::RequestPasskey(device::BluetoothDevice* device) {
  VLOG(1) << "RequestPassKey id = " << device->GetDeviceID()
          << " name = " << device->GetNameForDisplay();
  device->CancelPairing();
}

void HIDDetectionScreen::DisplayPinCode(device::BluetoothDevice* device,
                                        const std::string& pincode) {
  VLOG(1) << "DisplayPinCode id = " << device->GetDeviceID()
          << " name = " << device->GetNameForDisplay();
  GetContextEditor().SetString(kContextKeyPinCode, pincode);
  SetKeyboardDeviceName_(base::UTF16ToUTF8(device->GetNameForDisplay()));
  SendKeyboardDeviceNotification();
}

void HIDDetectionScreen::DisplayPasskey(device::BluetoothDevice* device,
                                        uint32_t passkey) {
  VLOG(1) << "DisplayPassKey id = " << device->GetDeviceID()
          << " name = " << device->GetNameForDisplay();
  std::string pincode = base::UintToString(passkey);
  pincode = std::string(kPincodeLength - pincode.length(), '0').append(pincode);
  // No differences in UI for passkey and pincode authentication calls.
  DisplayPinCode(device, pincode);
}

void HIDDetectionScreen::KeysEntered(device::BluetoothDevice* device,
                                     uint32_t entered) {
  VLOG(1) << "Number of keys entered " << entered;
  GetContextEditor()
      .SetBoolean(kContextKeyNumKeysEnteredExpected, true)
      .SetInteger(kContextKeyNumKeysEnteredPinCode, entered);
  SendKeyboardDeviceNotification();
}

void HIDDetectionScreen::ConfirmPasskey(device::BluetoothDevice* device,
                                        uint32_t passkey) {
  VLOG(1) << "Confirm Passkey";
  device->CancelPairing();
}

void HIDDetectionScreen::AuthorizePairing(device::BluetoothDevice* device) {
  // There is never any circumstance where this will be called, since the
  // HID detection screen will only be used for outgoing pairing
  // requests, but play it safe.
  VLOG(1) << "Authorize pairing";
  device->ConfirmPairing();
}

void HIDDetectionScreen::AdapterPresentChanged(
    device::BluetoothAdapter* adapter, bool present) {
  if (present && switch_on_adapter_when_ready_) {
    VLOG(1) << "Switching on BT adapter on HID OOBE screen.";
    adapter_initially_powered_.reset(new bool(adapter_->IsPowered()));
    adapter_->SetPowered(
        true,
        base::Bind(&HIDDetectionScreen::StartBTDiscoverySession,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&HIDDetectionScreen::SetPoweredError,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void HIDDetectionScreen::TryPairingAsPointingDevice(
    device::BluetoothDevice* device) {
  if (pointing_device_id_.empty() &&
      DeviceIsPointing(device->GetDeviceType()) &&
      device->IsPairable() &&
      !(device->IsConnected() && device->IsPaired()) &&
      !mouse_is_pairing_) {
    ConnectBTDevice(device);
  }
}

void HIDDetectionScreen::TryPairingAsKeyboardDevice(
    device::BluetoothDevice* device) {
  if (keyboard_device_id_.empty() &&
      DeviceIsKeyboard(device->GetDeviceType()) &&
      device->IsPairable() &&
      !(device->IsConnected() && device->IsPaired()) &&
      !keyboard_is_pairing_) {
    ConnectBTDevice(device);
  }
}

void HIDDetectionScreen::ConnectBTDevice(device::BluetoothDevice* device) {
  bool device_busy = (device->IsConnected() && device->IsPaired()) ||
                      device->IsConnecting();
  if (!device->IsPairable() || device_busy)
    return;
  device::BluetoothDeviceType device_type = device->GetDeviceType();

  if (device_type == device::BluetoothDeviceType::MOUSE ||
      device_type == device::BluetoothDeviceType::TABLET) {
    if (mouse_is_pairing_)
      return;
    mouse_is_pairing_ = true;
  } else if (device_type == device::BluetoothDeviceType::KEYBOARD) {
    if (keyboard_is_pairing_)
      return;
    keyboard_is_pairing_ = true;
  } else if (device_type == device::BluetoothDeviceType::KEYBOARD_MOUSE_COMBO) {
    if (mouse_is_pairing_ && keyboard_is_pairing_)
      return;
    mouse_is_pairing_ = true;
    keyboard_is_pairing_ = true;
  }
  device->Connect(this,
            base::Bind(&HIDDetectionScreen::BTConnected,
                       weak_ptr_factory_.GetWeakPtr(), device_type),
            base::Bind(&HIDDetectionScreen::BTConnectError,
                       weak_ptr_factory_.GetWeakPtr(),
                       device->GetAddress(), device_type));
}

void HIDDetectionScreen::BTConnected(device::BluetoothDeviceType device_type) {
  if (DeviceIsPointing(device_type))
    mouse_is_pairing_ = false;
  if (DeviceIsKeyboard(device_type)) {
    keyboard_is_pairing_ = false;
    GetContextEditor()
        .SetBoolean(kContextKeyNumKeysEnteredExpected, false)
        .SetString(kContextKeyPinCode, "");
    SendKeyboardDeviceNotification();
  }
}

void HIDDetectionScreen::BTConnectError(
    const std::string& address,
    device::BluetoothDeviceType device_type,
    device::BluetoothDevice::ConnectErrorCode error_code) {
  LOG(WARNING) << "BTConnectError while connecting " << address
               << " error code = " << error_code;
  if (DeviceIsPointing(device_type))
    mouse_is_pairing_ = false;
  if (DeviceIsKeyboard(device_type)) {
    keyboard_is_pairing_ = false;
    GetContextEditor()
        .SetInteger(kContextKeyNumKeysEnteredExpected, false)
        .SetString(kContextKeyPinCode, "");
    SendKeyboardDeviceNotification();
  }

  if (pointing_device_id_.empty() || keyboard_device_id_.empty())
    UpdateDevices();
}

void HIDDetectionScreen::SendPointingDeviceNotification() {
  std::string state;
  if (pointing_device_id_.empty())
    state = kSearchingState;
  else if (pointing_device_connect_type_ == InputDeviceInfo::TYPE_BLUETOOTH)
    state = kBTPairedState;
  else if (pointing_device_connect_type_ == InputDeviceInfo::TYPE_USB)
    state = kUSBState;
  else
    state = kConnectedState;
  GetContextEditor().SetString(kContextKeyMouseState, state)
                    .SetBoolean(
      kContextKeyContinueButtonEnabled,
      !(pointing_device_id_.empty() && keyboard_device_id_.empty()));
}

void HIDDetectionScreen::SendKeyboardDeviceNotification() {
  ContextEditor editor = GetContextEditor();
  editor.SetString(kContextKeyKeyboardLabel, "");
  if (keyboard_device_id_.empty()) {
    if (keyboard_is_pairing_) {
      editor.SetString(kContextKeyKeyboardState, kBTPairingState)
            .SetString(
          kContextKeyKeyboardLabel,
          l10n_util::GetStringFUTF8(
              IDS_HID_DETECTION_BLUETOOTH_REMOTE_PIN_CODE_REQUEST,
              base::UTF8ToUTF16(keyboard_device_name_)));
    } else {
      editor.SetString(kContextKeyKeyboardState, kSearchingState);
    }
  } else {
    if (keyboard_device_connect_type_ == InputDeviceInfo::TYPE_BLUETOOTH) {
      editor.SetString(kContextKeyKeyboardState, kBTPairedState)
            .SetString(
                kContextKeyKeyboardLabel,
                l10n_util::GetStringFUTF16(
                    IDS_HID_DETECTION_PAIRED_BLUETOOTH_KEYBOARD,
                    base::UTF8ToUTF16(keyboard_device_name_)));
    } else {
      editor.SetString(kContextKeyKeyboardState, kUSBState);
    }
  }
  editor.SetString(kContextKeyKeyboardDeviceName, keyboard_device_name_)
        .SetBoolean(
            kContextKeyContinueButtonEnabled,
            !(pointing_device_id_.empty() && keyboard_device_id_.empty()));
}

void HIDDetectionScreen::SetKeyboardDeviceName_(const std::string& name) {
  keyboard_device_name_ =
      keyboard_device_id_.empty() || !name.empty()
          ? name
          : l10n_util::GetStringUTF8(IDS_HID_DETECTION_DEFAULT_KEYBOARD_NAME);
}

void HIDDetectionScreen::DeviceAdded(
    device::BluetoothAdapter* adapter, device::BluetoothDevice* device) {
  VLOG(1) << "BT input device added id = " << device->GetDeviceID()
          << " name = " << device->GetNameForDisplay();
  TryPairingAsPointingDevice(device);
  TryPairingAsKeyboardDevice(device);
}

void HIDDetectionScreen::DeviceChanged(
    device::BluetoothAdapter* adapter, device::BluetoothDevice* device) {
  VLOG(1) << "BT device changed id = " << device->GetDeviceID()
          << " name = " << device->GetNameForDisplay();
  TryPairingAsPointingDevice(device);
  TryPairingAsKeyboardDevice(device);
}

void HIDDetectionScreen::DeviceRemoved(
    device::BluetoothAdapter* adapter, device::BluetoothDevice* device) {
  VLOG(1) << "BT device removed id = " << device->GetDeviceID()
          << " name = " << device->GetNameForDisplay();
}

void HIDDetectionScreen::OnInputDeviceAdded(
    const InputDeviceInfo& info) {
  VLOG(1) << "Input device added id = " << info.id << " name = " << info.name;
  // TODO(merkulova): deal with all available device types, e.g. joystick.
  if (!keyboard_device_id_.empty() && !pointing_device_id_.empty())
    return;

  if (pointing_device_id_.empty() && DeviceIsPointing(info)) {
    pointing_device_id_ = info.id;
    GetContextEditor().SetString(kContextKeyMouseDeviceName, info.name);
    pointing_device_connect_type_ = info.type;
    SendPointingDeviceNotification();
  }
  if (keyboard_device_id_.empty() && info.is_keyboard) {
    keyboard_device_id_ = info.id;
    keyboard_device_connect_type_ = info.type;
    SetKeyboardDeviceName_(info.name);
    SendKeyboardDeviceNotification();
  }
}

void HIDDetectionScreen::OnInputDeviceRemoved(const std::string& id) {
  if (id == keyboard_device_id_) {
    keyboard_device_id_.clear();
    keyboard_device_connect_type_ = InputDeviceInfo::TYPE_UNKNOWN;
    SendKeyboardDeviceNotification();
    UpdateDevices();
  }
  if (id == pointing_device_id_) {
    pointing_device_id_.clear();
    pointing_device_connect_type_ = InputDeviceInfo::TYPE_UNKNOWN;
    SendPointingDeviceNotification();
    UpdateDevices();
  }
}

void HIDDetectionScreen::InitializeAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  CHECK(adapter_.get());

  adapter_->AddObserver(this);
  UpdateDevices();
}

void HIDDetectionScreen::StartBTDiscoverySession() {
  adapter_->StartDiscoverySession(
      base::Bind(&HIDDetectionScreen::OnStartDiscoverySession,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&HIDDetectionScreen::FindDevicesError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void HIDDetectionScreen::ProcessConnectedDevicesList(
    const std::vector<InputDeviceInfo>& devices) {
  for (std::vector<InputDeviceInfo>::const_iterator it = devices.begin();
       it != devices.end() &&
       (pointing_device_id_.empty() || keyboard_device_id_.empty());
       ++it) {
    if (pointing_device_id_.empty() && DeviceIsPointing(*it)) {
      pointing_device_id_ = it->id;
      GetContextEditor().SetString(kContextKeyMouseDeviceName, it->name);
      pointing_device_connect_type_ = it->type;
      SendPointingDeviceNotification();
    }
    if (keyboard_device_id_.empty() && it->is_keyboard) {
      keyboard_device_id_ = it->id;
      SetKeyboardDeviceName_(it->name);
      keyboard_device_connect_type_ = it->type;
      SendKeyboardDeviceNotification();
    }
  }
}

void HIDDetectionScreen::TryInitiateBTDevicesUpdate() {
  if ((pointing_device_id_.empty() || keyboard_device_id_.empty()) &&
      adapter_.get()) {
    if (!adapter_->IsPresent()) {
      // Switch on BT adapter later when it's available.
      switch_on_adapter_when_ready_ = true;
    } else if (!adapter_->IsPowered()) {
      VLOG(1) << "Switching on BT adapter on HID OOBE screen.";
      adapter_initially_powered_.reset(new bool(false));
      adapter_->SetPowered(
          true,
          base::Bind(&HIDDetectionScreen::StartBTDiscoverySession,
                     weak_ptr_factory_.GetWeakPtr()),
          base::Bind(&HIDDetectionScreen::SetPoweredError,
                     weak_ptr_factory_.GetWeakPtr()));
    } else {
      UpdateBTDevices();
    }
  }
}

void HIDDetectionScreen::OnGetInputDevicesListForCheck(
    const base::Callback<void(bool)>& on_check_done,
    const std::vector<InputDeviceInfo>& devices) {
  ProcessConnectedDevicesList(devices);

  // Screen is not required if both devices are present.
  bool all_devices_autodetected = !pointing_device_id_.empty() &&
                                  !keyboard_device_id_.empty();
  UMA_HISTOGRAM_BOOLEAN("HIDDetection.OOBEDialogShown",
                        !all_devices_autodetected);

  on_check_done.Run(!all_devices_autodetected);
}

void HIDDetectionScreen::OnGetInputDevicesList(
    const std::vector<InputDeviceInfo>& devices) {
  ProcessConnectedDevicesList(devices);
  TryInitiateBTDevicesUpdate();
}

void HIDDetectionScreen::UpdateDevices() {
  input_service_proxy_.GetDevices(
      base::Bind(&HIDDetectionScreen::OnGetInputDevicesList,
                 weak_ptr_factory_.GetWeakPtr()));
}

void HIDDetectionScreen::UpdateBTDevices() {
  if (!adapter_.get() || !adapter_->IsPresent() || !adapter_->IsPowered())
    return;

  // If no connected devices found as pointing device and keyboard, we try to
  // connect some type-suitable active bluetooth device.
  std::vector<device::BluetoothDevice*> bt_devices = adapter_->GetDevices();
  for (std::vector<device::BluetoothDevice*>::const_iterator it =
           bt_devices.begin();
       it != bt_devices.end() &&
           (keyboard_device_id_.empty() || pointing_device_id_.empty());
       ++it) {
    TryPairingAsPointingDevice(*it);
    TryPairingAsKeyboardDevice(*it);
  }
}

void HIDDetectionScreen::OnStartDiscoverySession(
    std::unique_ptr<device::BluetoothDiscoverySession> discovery_session) {
  VLOG(1) << "BT Discovery session started";
  discovery_session_ = std::move(discovery_session);
  UpdateDevices();
}

void HIDDetectionScreen::OnGetInputDevicesForPowerOff(
    const std::vector<InputDeviceInfo>& devices) {
  bool use_bluetooth = false;
  for (const auto& device : devices) {
    if (device.type == InputDeviceInfo::TYPE_BLUETOOTH) {
      use_bluetooth = true;
      break;
    }
  }
  if (!use_bluetooth) {
    VLOG(1) << "Switching off BT adapter after HID OOBE screen as unused.";
    adapter_->SetPowered(false, base::Bind(&base::DoNothing),
                         base::Bind(&HIDDetectionScreen::SetPoweredOffError,
                                    weak_ptr_factory_.GetWeakPtr()));
  }
}

void HIDDetectionScreen::SetPoweredError() {
  LOG(ERROR) << "Failed to power BT adapter";
}

void HIDDetectionScreen::SetPoweredOffError() {
  LOG(ERROR) << "Failed to power off BT adapter";
}

void HIDDetectionScreen::FindDevicesError() {
  VLOG(1) << "Failed to start Bluetooth discovery.";
}

scoped_refptr<device::BluetoothAdapter>
HIDDetectionScreen::GetAdapterForTesting() {
  return adapter_;
}

void HIDDetectionScreen::SetAdapterInitialPoweredForTesting(bool powered) {
  adapter_initially_powered_.reset(new bool(powered));
}

}  // namespace chromeos
