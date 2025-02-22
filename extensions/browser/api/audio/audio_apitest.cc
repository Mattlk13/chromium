// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "extensions/shell/test/shell_apitest.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"

#if defined(OS_CHROMEOS)
#include "chromeos/audio/audio_devices_pref_handler_stub.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cras_audio_client.h"
#endif

namespace extensions {

#if defined(OS_CHROMEOS)
using chromeos::AudioDevice;
using chromeos::AudioDeviceList;
using chromeos::AudioNode;
using chromeos::AudioNodeList;

const uint64_t kJabraSpeaker1Id = 30001;
const uint64_t kJabraSpeaker1StableDeviceId = 80001;
const uint64_t kJabraSpeaker2Id = 30002;
const uint64_t kJabraSpeaker2StableDeviceId = 80002;
const uint64_t kHDMIOutputId = 30003;
const uint64_t kHDMIOutputStabeDevicelId = 80003;
const uint64_t kJabraMic1Id = 40001;
const uint64_t kJabraMic1StableDeviceId = 90001;
const uint64_t kJabraMic2Id = 40002;
const uint64_t kJabraMic2StableDeviceId = 90002;
const uint64_t kWebcamMicId = 40003;
const uint64_t kWebcamMicStableDeviceId = 90003;

struct AudioNodeInfo {
  bool is_input;
  uint64_t id;
  uint64_t stable_id;
  const char* const device_name;
  const char* const type;
  const char* const name;
};

const AudioNodeInfo kJabraSpeaker1 = {
    false, kJabraSpeaker1Id, kJabraSpeaker1StableDeviceId, "Jabra Speaker",
    "USB", "Jabra Speaker 1"};

const AudioNodeInfo kJabraSpeaker2 = {
    false, kJabraSpeaker2Id, kJabraSpeaker2StableDeviceId, "Jabra Speaker",
    "USB", "Jabra Speaker 2"};

const AudioNodeInfo kHDMIOutput = {
    false,         kHDMIOutputId, kHDMIOutputStabeDevicelId,
    "HDMI output", "HDMI",        "HDA Intel MID"};

const AudioNodeInfo kJabraMic1 = {
    true,        kJabraMic1Id, kJabraMic1StableDeviceId,
    "Jabra Mic", "USB",        "Jabra Mic 1"};

const AudioNodeInfo kJabraMic2 = {
    true,        kJabraMic2Id, kJabraMic2StableDeviceId,
    "Jabra Mic", "USB",        "Jabra Mic 2"};

const AudioNodeInfo kUSBCameraMic = {
    true,         kWebcamMicId, kWebcamMicStableDeviceId,
    "Webcam Mic", "USB",        "Logitech Webcam"};

AudioNode CreateAudioNode(const AudioNodeInfo& info, int version) {
  return AudioNode(info.is_input, info.id, version == 2,
                   // stable_device_id:
                   info.stable_id ^ (version == 1 ? 0 : 0xFFFF),
                   // stable_device_id_old:
                   version == 1 ? 0 : info.stable_id, info.device_name,
                   info.type, info.name, false, 0);
}

class AudioApiTest : public ShellApiTest {
 public:
  AudioApiTest() : cras_audio_handler_(NULL), fake_cras_audio_client_(NULL) {}
  ~AudioApiTest() override {}

  void SetUpCrasAudioHandlerWithTestingNodes(const AudioNodeList& audio_nodes) {
    chromeos::DBusThreadManager* dbus_manager =
        chromeos::DBusThreadManager::Get();
    DCHECK(dbus_manager);
    fake_cras_audio_client_ = static_cast<chromeos::FakeCrasAudioClient*>(
        dbus_manager->GetCrasAudioClient());
    fake_cras_audio_client_->SetAudioNodesAndNotifyObserversForTesting(
        audio_nodes);
    cras_audio_handler_ = chromeos::CrasAudioHandler::Get();
    DCHECK(cras_audio_handler_);
    base::RunLoop().RunUntilIdle();
  }

  void ChangeAudioNodes(const AudioNodeList& audio_nodes) {
    DCHECK(fake_cras_audio_client_);
    fake_cras_audio_client_->SetAudioNodesAndNotifyObserversForTesting(
        audio_nodes);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::MessageLoopForUI message_loop_;
  chromeos::CrasAudioHandler* cras_audio_handler_;  // Not owned.
  chromeos::FakeCrasAudioClient* fake_cras_audio_client_;  // Not owned.
};

IN_PROC_BROWSER_TEST_F(AudioApiTest, AudioV1StableId) {
  // Set up the audio nodes for testing.
  AudioNodeList audio_nodes = {
      CreateAudioNode(kJabraSpeaker1, 1), CreateAudioNode(kJabraSpeaker2, 1),
      CreateAudioNode(kHDMIOutput, 1),    CreateAudioNode(kJabraMic1, 1),
      CreateAudioNode(kJabraMic2, 1),     CreateAudioNode(kUSBCameraMic, 1)};

  SetUpCrasAudioHandlerWithTestingNodes(audio_nodes);

  EXPECT_TRUE(RunAppTest("api_test/audio")) << message_;
}

IN_PROC_BROWSER_TEST_F(AudioApiTest, AudioV2StableId) {
  // Set up the audio nodes for testing.
  AudioNodeList audio_nodes = {
      CreateAudioNode(kJabraSpeaker1, 2), CreateAudioNode(kJabraSpeaker2, 2),
      CreateAudioNode(kHDMIOutput, 2),    CreateAudioNode(kJabraMic1, 2),
      CreateAudioNode(kJabraMic2, 2),     CreateAudioNode(kUSBCameraMic, 2)};

  SetUpCrasAudioHandlerWithTestingNodes(audio_nodes);

  EXPECT_TRUE(RunAppTest("api_test/audio")) << message_;
}

IN_PROC_BROWSER_TEST_F(AudioApiTest, OnLevelChangedOutputDevice) {
  AudioNodeList audio_nodes = {CreateAudioNode(kJabraSpeaker1, 1),
                               CreateAudioNode(kHDMIOutput, 1)};
  SetUpCrasAudioHandlerWithTestingNodes(audio_nodes);

  // Verify the jabra speaker is the active output device.
  AudioDevice device;
  EXPECT_TRUE(cras_audio_handler_->GetPrimaryActiveOutputDevice(&device));
  EXPECT_EQ(device.id, kJabraSpeaker1.id);

  // Loads background app.
  ResultCatcher result_catcher;
  ExtensionTestMessageListener load_listener("loaded", false);
  ASSERT_TRUE(LoadApp("api_test/audio/volume_change"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Change output device volume.
  const int kVolume = 60;
  cras_audio_handler_->SetOutputVolumePercent(kVolume);

  // Verify the output volume is changed to the designated value.
  EXPECT_EQ(kVolume, cras_audio_handler_->GetOutputVolumePercent());
  EXPECT_EQ(kVolume,
            cras_audio_handler_->GetOutputVolumePercentForDevice(device.id));

  // Verify the background app got the OnOutputNodeVolumeChanged event
  // with the expected node id and volume value.
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(AudioApiTest, OnOutputMuteChanged) {
  AudioNodeList audio_nodes = {CreateAudioNode(kJabraSpeaker1, 1),
                               CreateAudioNode(kHDMIOutput, 1)};
  SetUpCrasAudioHandlerWithTestingNodes(audio_nodes);

  // Verify the jabra speaker is the active output device.
  AudioDevice device;
  EXPECT_TRUE(cras_audio_handler_->GetPrimaryActiveOutputDevice(&device));
  EXPECT_EQ(device.id, kJabraSpeaker1.id);

  // Mute the output.
  cras_audio_handler_->SetOutputMute(true);
  EXPECT_TRUE(cras_audio_handler_->IsOutputMuted());

  // Loads background app.
  ResultCatcher result_catcher;
  ExtensionTestMessageListener load_listener("loaded", false);
  ASSERT_TRUE(LoadApp("api_test/audio/output_mute_change"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Un-mute the output.
  cras_audio_handler_->SetOutputMute(false);
  EXPECT_FALSE(cras_audio_handler_->IsOutputMuted());

  // Verify the background app got the OnMuteChanged event
  // with the expected output un-muted state.
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(AudioApiTest, OnInputMuteChanged) {
  AudioNodeList audio_nodes = {CreateAudioNode(kJabraMic1, 1),
                               CreateAudioNode(kUSBCameraMic, 1)};
  SetUpCrasAudioHandlerWithTestingNodes(audio_nodes);

  // Set the jabra mic to be the active input device.
  AudioDevice jabra_mic(CreateAudioNode(kJabraMic1, 1));
  cras_audio_handler_->SwitchToDevice(
      jabra_mic, true, chromeos::CrasAudioHandler::ACTIVATE_BY_USER);
  EXPECT_EQ(kJabraMic1.id, cras_audio_handler_->GetPrimaryActiveInputNode());

  // Un-mute the input.
  cras_audio_handler_->SetInputMute(false);
  EXPECT_FALSE(cras_audio_handler_->IsInputMuted());

  // Loads background app.
  ResultCatcher result_catcher;
  ExtensionTestMessageListener load_listener("loaded", false);
  ASSERT_TRUE(LoadApp("api_test/audio/input_mute_change"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Mute the input.
  cras_audio_handler_->SetInputMute(true);
  EXPECT_TRUE(cras_audio_handler_->IsInputMuted());

  // Verify the background app got the OnMuteChanged event
  // with the expected input muted state.
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(AudioApiTest, OnNodesChangedAddNodes) {
  AudioNodeList audio_nodes = {CreateAudioNode(kJabraSpeaker1, 1),
                               CreateAudioNode(kJabraSpeaker2, 1)};
  SetUpCrasAudioHandlerWithTestingNodes(audio_nodes);
  const size_t init_device_size = audio_nodes.size();

  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_device_size, audio_devices.size());

  // Load background app.
  ResultCatcher result_catcher;
  ExtensionTestMessageListener load_listener("loaded", false);
  ASSERT_TRUE(LoadApp("api_test/audio/add_nodes"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Plug in HDMI output.
  audio_nodes.push_back(CreateAudioNode(kHDMIOutput, 1));
  ChangeAudioNodes(audio_nodes);
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_device_size + 1, audio_devices.size());

  // Verify the background app got the OnNodesChanged event
  // with the new node added.
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

IN_PROC_BROWSER_TEST_F(AudioApiTest, OnNodesChangedRemoveNodes) {
  AudioNodeList audio_nodes = {CreateAudioNode(kJabraMic1, 1),
                               CreateAudioNode(kJabraMic2, 1),
                               CreateAudioNode(kUSBCameraMic, 1)};
  SetUpCrasAudioHandlerWithTestingNodes(audio_nodes);
  const size_t init_device_size = audio_nodes.size();

  AudioDeviceList audio_devices;
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_device_size, audio_devices.size());

  // Load background app.
  ResultCatcher result_catcher;
  ExtensionTestMessageListener load_listener("loaded", false);
  ASSERT_TRUE(LoadApp("api_test/audio/remove_nodes"));
  ASSERT_TRUE(load_listener.WaitUntilSatisfied());

  // Remove camera mic.
  audio_nodes.erase(audio_nodes.begin() + init_device_size - 1);
  ChangeAudioNodes(audio_nodes);
  cras_audio_handler_->GetAudioDevices(&audio_devices);
  EXPECT_EQ(init_device_size - 1, audio_devices.size());

  // Verify the background app got the onNodesChanged event
  // with the last node removed.
  EXPECT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
}

#endif  // OS_CHROMEOS

}  // namespace extensions
