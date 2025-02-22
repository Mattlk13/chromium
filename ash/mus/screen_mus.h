// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_SCREEN_MUS_H_
#define ASH_MUS_SCREEN_MUS_H_

#include "base/macros.h"
#include "ui/display/screen_base.h"

namespace ash {

// Implementation of Screen for mus. The WindowManager/RootWindowController
// keep the set of displays in sync (by modifying display_list()). This class
// exists to provide implementations of ScreenBase functions that are
// NOTIMPLEMENTED.
// TODO(sky): implement remaining functions or merge with ScreenAsh:
// http://crbug.com/671408.
class ScreenMus : public display::ScreenBase {
 public:
  ScreenMus();
  ~ScreenMus() override;

 private:
  // display::ScreenBase:
  display::Display GetDisplayNearestWindow(aura::Window* window) const override;
  gfx::Point GetCursorScreenPoint() override;

  DISALLOW_COPY_AND_ASSIGN(ScreenMus);
};

}  // namespace ash

#endif  // ASH_MUS_SCREEN_MUS_H_
