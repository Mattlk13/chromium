// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_WM_ROOT_WINDOW_CONTROLLER_MUS_H_
#define ASH_MUS_BRIDGE_WM_ROOT_WINDOW_CONTROLLER_MUS_H_

#include "ash/common/wm_root_window_controller.h"
#include "base/macros.h"

namespace aura {
class Window;
}

namespace display {
class Display;
}

namespace ash {
namespace mus {

class RootWindowController;
class WmShellMus;
class WmWindowMus;

// WmRootWindowController implementations for mus.
class WmRootWindowControllerMus : public WmRootWindowController {
 public:
  WmRootWindowControllerMus(WmShellMus* shell,
                            RootWindowController* root_window_controller);
  ~WmRootWindowControllerMus() override;

  static WmRootWindowControllerMus* Get(aura::Window* window) {
    return const_cast<WmRootWindowControllerMus*>(
        Get(const_cast<const aura::Window*>(window)));
  }
  static const WmRootWindowControllerMus* Get(const aura::Window* window);

  RootWindowController* root_window_controller() {
    return root_window_controller_;
  }

  // Screen conversion functions.
  gfx::Point ConvertPointToScreen(const WmWindowMus* source,
                                  const gfx::Point& point) const;

  const display::Display& GetDisplay() const;

  // Exposed as public so WindowManager can call it.
  void MoveWindowsTo(WmWindow* dest);

  // WmRootWindowController:
  bool HasShelf() override;
  WmShell* GetShell() override;
  WmShelf* GetShelf() override;
  WmWindow* GetWindow() override;

 private:
  friend class RootWindowController;

  // WmRootWindowController:
  bool ShouldDestroyWindowInCloseChildWindows(WmWindow* window) override;

  WmShellMus* shell_;
  RootWindowController* root_window_controller_;

  DISALLOW_COPY_AND_ASSIGN(WmRootWindowControllerMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_WM_ROOT_WINDOW_CONTROLLER_MUS_H_
