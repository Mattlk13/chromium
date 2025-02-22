// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IME_DRIVER_INPUT_METHOD_BRIDGE_CHROMEOS_H_
#define CHROME_BROWSER_UI_VIEWS_IME_DRIVER_INPUT_METHOD_BRIDGE_CHROMEOS_H_

#include "services/ui/public/interfaces/ime/ime.mojom.h"
#include "ui/base/ime/input_method_chromeos.h"

// This bridges between mojo InputMethod API and ui::InputMethodChromeOS. It
// forwards the received events to an instance of ui::InputMethodChromeOS.
class InputMethodBridge : public ui::mojom::InputMethod {
 public:
  explicit InputMethodBridge(ui::mojom::TextInputClientPtr client);
  ~InputMethodBridge() override;

  // ui::mojom::InputMethod:
  void OnTextInputModeChanged(
      ui::mojom::TextInputMode text_input_mode) override;
  void OnTextInputTypeChanged(
      ui::mojom::TextInputType text_input_type) override;
  void OnCaretBoundsChanged(const gfx::Rect& caret_bounds) override;
  void ProcessKeyEvent(std::unique_ptr<ui::Event> key_event,
                       const ProcessKeyEventCallback& callback) override;
  void CancelComposition() override;

 private:
  std::unique_ptr<ui::TextInputClient> client_;
  std::unique_ptr<ui::InputMethodChromeOS> input_method_chromeos_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodBridge);
};

#endif  // CHROME_BROWSER_UI_VIEWS_IME_DRIVER_INPUT_METHOD_BRIDGE_CHROMEOS_H_
