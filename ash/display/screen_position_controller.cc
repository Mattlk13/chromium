// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/screen_position_controller.h"

#include "ash/aura/wm_window_aura.h"
#include "ash/common/wm/window_positioning_utils.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/window_properties.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/dip_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/wm/core/window_util.h"

namespace ash {

// static
void ScreenPositionController::ConvertHostPointToRelativeToRootWindow(
    aura::Window* root_window,
    const aura::Window::Windows& root_windows,
    gfx::Point* point,
    aura::Window** target_root) {
  DCHECK(!root_window->parent());
  gfx::Point point_in_root(*point);
  root_window->GetHost()->ConvertPixelsToDIP(&point_in_root);

#if defined(USE_X11) || defined(USE_OZONE)
  gfx::Rect host_bounds(root_window->GetHost()->GetBoundsInPixels().size());
  if (!host_bounds.Contains(*point)) {
    // This conversion is necessary to deal with X's passive input
    // grab while dragging window. For example, if we have two
    // displays, say 1000x1000 (primary) and 500x500 (extended one
    // on the right), and start dragging a window at (999, 123), and
    // then move the pointer to the right, the pointer suddenly
    // warps to the extended display. The destination is (0, 123) in
    // the secondary root window's coordinates, or (1000, 123) in
    // the screen coordinates. However, since the mouse is captured
    // by X during drag, a weird LocatedEvent, something like (0, 1123)
    // in the *primary* root window's coordinates, is sent to Chrome
    // (Remember that in the native X11 world, the two root windows
    // are always stacked vertically regardless of the display
    // layout in Ash). We need to figure out that (0, 1123) in the
    // primary root window's coordinates is actually (0, 123) in the
    // extended root window's coordinates.
    //
    // For now Ozone works in a similar manner as X11. Transitioning from one
    // display's coordinate system to anothers may cause events in the
    // primary's coordinate system which fall in the extended display.

    gfx::Point location_in_native(point_in_root);

    root_window->GetHost()->ConvertDIPToScreenInPixels(&location_in_native);

    for (size_t i = 0; i < root_windows.size(); ++i) {
      aura::WindowTreeHost* host = root_windows[i]->GetHost();
      const gfx::Rect native_bounds = host->GetBoundsInPixels();
      if (native_bounds.Contains(location_in_native)) {
        *target_root = root_windows[i];
        *point = location_in_native;
        host->ConvertScreenInPixelsToDIP(point);
        return;
      }
    }
  }
#endif
  *target_root = root_window;
  *point = point_in_root;
}

void ScreenPositionController::ConvertPointToScreen(const aura::Window* window,
                                                    gfx::Point* point) {
  const aura::Window* root = window->GetRootWindow();
  aura::Window::ConvertPointToTarget(window, root, point);
  const gfx::Point display_origin =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(const_cast<aura::Window*>(root))
          .bounds()
          .origin();
  point->Offset(display_origin.x(), display_origin.y());
}

void ScreenPositionController::ConvertPointFromScreen(
    const aura::Window* window,
    gfx::Point* point) {
  const aura::Window* root = window->GetRootWindow();
  const gfx::Point display_origin =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(const_cast<aura::Window*>(root))
          .bounds()
          .origin();
  point->Offset(-display_origin.x(), -display_origin.y());
  aura::Window::ConvertPointToTarget(root, window, point);
}

void ScreenPositionController::ConvertHostPointToScreen(
    aura::Window* root_window,
    gfx::Point* point) {
  aura::Window* root = root_window->GetRootWindow();
  aura::Window* target_root = nullptr;
  ConvertHostPointToRelativeToRootWindow(
      root, WmWindowAura::ToAuraWindows(WmShell::Get()->GetAllRootWindows()),
      point, &target_root);
  ConvertPointToScreen(target_root, point);
}

void ScreenPositionController::SetBounds(aura::Window* window,
                                         const gfx::Rect& bounds,
                                         const display::Display& display) {
  if (!window->parent()->GetProperty(kUsesScreenCoordinatesKey)) {
    window->SetBounds(bounds);
    return;
  }

  wm::SetBoundsInScreen(WmWindowAura::Get(window), bounds, display);
}

}  // namespace ash
