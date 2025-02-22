// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/touch/touch_transformer_controller.h"

#include "ash/display/window_tree_host_manager.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/chromeos/display_configurator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/events/devices/device_data_manager.h"

namespace ash {

namespace {

display::DisplayManager* GetDisplayManager() {
  return Shell::GetInstance()->display_manager();
}

ui::TouchscreenDevice FindTouchscreenById(int id) {
  const std::vector<ui::TouchscreenDevice>& touchscreens =
      ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices();
  for (const auto& touchscreen : touchscreens) {
    if (touchscreen.id == id)
      return touchscreen;
  }

  return ui::TouchscreenDevice();
}

// Given an array of touch point and display point pairs, this function computes
// and returns the constants(defined below) using a least fit algorithm.
// If (xt, yt) is a touch point then its corresponding (xd, yd) would be defined
// by the following 2 equations:
// xd = xt * A + yt * B + C
// yd = xt * D + yt * E + F
// This function computes A, B, C, D, E and F and sets |ctm| with the calibrated
// transform matrix. In case the computation fails, the function will return
// false.
// See http://crbug.com/672293
bool GetCalibratedTransform(
    std::array<std::pair<gfx::Point, gfx::Point>, 4> touch_point_pairs,
    const gfx::Transform& pre_calibration_tm,
    gfx::Transform* ctm) {
  // Transform the display points before solving the equation.
  // If the calibration was performed at a resolution that is 0.5 times the
  // current resolution, then the display points (x, y) for a given touch point
  // now represents a display point at (2 * x, 2 * y). This and other kinds of
  // similar tranforms can be applied using |pre_calibration_tm|.
  for (int row = 0; row < 4; row++)
    pre_calibration_tm.TransformPoint(&touch_point_pairs[row].first);

  // Vector of the X-coordinate of display points corresponding to each of the
  // touch points.
  SkVector4 display_points_x(
      touch_point_pairs[0].first.x(), touch_point_pairs[1].first.x(),
      touch_point_pairs[2].first.x(), touch_point_pairs[3].first.x());
  // Vector of the Y-coordinate of display points corresponding to each of the
  // touch points.
  SkVector4 display_points_y(
      touch_point_pairs[0].first.y(), touch_point_pairs[1].first.y(),
      touch_point_pairs[2].first.y(), touch_point_pairs[3].first.y());

  // Initialize |touch_point_matrix|
  // If {(xt_1, yt_1), (xt_2, yt_2), (xt_3, yt_3)....} are a set of touch points
  // received during calibration, then the |touch_point_matrix| would be defined
  // as:
  // |xt_1  yt_1  1  0|
  // |xt_2  yt_2  1  0|
  // |xt_3  yt_3  1  0|
  // |xt_4  yt_4  1  0|
  SkMatrix44 touch_point_matrix;
  for (int row = 0; row < 4; row++) {
    touch_point_matrix.set(row, 0, touch_point_pairs[row].second.x());
    touch_point_matrix.set(row, 1, touch_point_pairs[row].second.y());
    touch_point_matrix.set(row, 2, 1);
    touch_point_matrix.set(row, 3, 0);
  }
  SkMatrix44 touch_point_matrix_transpose(touch_point_matrix);
  touch_point_matrix_transpose.transpose();

  SkMatrix44 product_matrix = touch_point_matrix_transpose * touch_point_matrix;

  // Set (3, 3) = 1 so that |determinent| of the matrix is != 0 and the inverse
  // can be calculated.
  product_matrix.set(3, 3, 1);

  SkMatrix44 product_matrix_inverse;

  // NOTE: If the determinent is zero then the inverse cannot be computed. The
  // only solution is to restart touch calibration and get new points from user.
  if (!product_matrix.invert(&product_matrix_inverse)) {
    NOTREACHED() << "Touch Calibration failed. Determinent is zero.";
    return false;
  }

  product_matrix_inverse.set(3, 3, 0);

  product_matrix = product_matrix_inverse * touch_point_matrix_transpose;

  // Constants [A, B, C, 0] used to calibrate the x-coordinate of touch input.
  // x_new = x_old * A + y_old * B + C;
  SkVector4 x_constants = product_matrix * display_points_x;
  // Constants [D, E, F, 0] used to calibrate the y-coordinate of touch input.
  // y_new = x_old * D + y_old * E + F;
  SkVector4 y_constants = product_matrix * display_points_y;

  // Create a transform matrix using the touch calibration data.
  ctm->ConcatTransform(gfx::Transform(
      x_constants.fData[0], x_constants.fData[1], 0, x_constants.fData[2],
      y_constants.fData[0], y_constants.fData[1], 0, y_constants.fData[2], 0, 0,
      1, 0, 0, 0, 0, 1));
  return true;
}

// Returns an uncalibrated touch transform.
gfx::Transform GetUncalibratedTransform(
    const gfx::Transform& tm,
    const display::ManagedDisplayInfo& display,
    const display::ManagedDisplayInfo& touch_display,
    const gfx::SizeF& touch_area,
    const gfx::SizeF& touch_native_size) {
  gfx::SizeF current_size(display.bounds_in_native().size());
  gfx::Transform ctm(tm);
  // Take care of panel fitting only if supported. Panel fitting is emulated
  // in software mirroring mode (display != touch_display).
  // If panel fitting is enabled then the aspect ratio is preserved and the
  // display is scaled acordingly. In this case blank regions would be present
  // in order to center the displayed area.
  if (display.is_aspect_preserving_scaling() ||
      display.id() != touch_display.id()) {
    float touch_calib_ar =
        touch_native_size.width() / touch_native_size.height();
    float current_ar = current_size.width() / current_size.height();

    if (current_ar > touch_calib_ar) {  // Letterboxing
      ctm.Translate(
          0, (1 - current_ar / touch_calib_ar) * 0.5 * current_size.height());
      ctm.Scale(1, current_ar / touch_calib_ar);
    } else if (touch_calib_ar > current_ar) {  // Pillarboxing
      ctm.Translate(
          (1 - touch_calib_ar / current_ar) * 0.5 * current_size.width(), 0);
      ctm.Scale(touch_calib_ar / current_ar, 1);
    }
  }
  // Take care of scaling between touchscreen area and display resolution.
  ctm.Scale(current_size.width() / touch_area.width(),
            current_size.height() / touch_area.height());
  return ctm;
}

}  // namespace

// This is to compute the scale ratio for the TouchEvent's radius. The
// configured resolution of the display is not always the same as the touch
// screen's reporting resolution, e.g. the display could be set as
// 1920x1080 while the touchscreen is reporting touch position range at
// 32767x32767. Touch radius is reported in the units the same as touch position
// so we need to scale the touch radius to be compatible with the display's
// resolution. We compute the scale as
// sqrt of (display_area / touchscreen_area)
double TouchTransformerController::GetTouchResolutionScale(
    const display::ManagedDisplayInfo& touch_display,
    const ui::TouchscreenDevice& touch_device) const {
  if (touch_device.id == ui::InputDevice::kInvalidId ||
      touch_device.size.IsEmpty() ||
      touch_display.bounds_in_native().size().IsEmpty())
    return 1.0;

  double display_area = touch_display.bounds_in_native().size().GetArea();
  double touch_area = touch_device.size.GetArea();
  double ratio = std::sqrt(display_area / touch_area);

  VLOG(2) << "Display size: "
          << touch_display.bounds_in_native().size().ToString()
          << ", Touchscreen size: " << touch_device.size.ToString()
          << ", Touch radius scale ratio: " << ratio;
  return ratio;
}

gfx::Transform TouchTransformerController::GetTouchTransform(
    const display::ManagedDisplayInfo& display,
    const display::ManagedDisplayInfo& touch_display,
    const ui::TouchscreenDevice& touchscreen,
    const gfx::Size& framebuffer_size) const {
  auto current_size = gfx::SizeF(display.bounds_in_native().size());
  auto touch_native_size = gfx::SizeF(touch_display.GetNativeModeSize());
#if defined(USE_OZONE)
  auto touch_area = gfx::SizeF(touchscreen.size);
#elif defined(USE_X11)
  // On X11 touches are reported in the framebuffer coordinate space.
  auto touch_area = gfx::SizeF(framebuffer_size);
#endif

  gfx::Transform ctm;

  if (current_size.IsEmpty() || touch_native_size.IsEmpty() ||
      touch_area.IsEmpty() || touchscreen.id == ui::InputDevice::kInvalidId)
    return ctm;

#if defined(USE_OZONE)
  // Translate the touch so that it falls within the display bounds. This
  // should not be performed if the displays are mirrored.
  if (display.id() == touch_display.id()) {
    ctm.Translate(display.bounds_in_native().x(),
                  display.bounds_in_native().y());
  }
#endif

  // If touch calibration data is unavailable, use naive approach.
  if (!touch_display.has_touch_calibration_data()) {
    return GetUncalibratedTransform(ctm, display, touch_display, touch_area,
                                    touch_native_size);
  }

  // The resolution at which the touch calibration was performed.
  gfx::SizeF touch_calib_size(touch_display.GetTouchCalibrationData().bounds);

  // Any additional transfomration that needs to be applied to the display
  // points, before we solve for the final transform.
  gfx::Transform pre_transform;

  if (display.id() != touch_display.id() ||
      display.is_aspect_preserving_scaling()) {
    // Case of displays being mirrored or in panel fitting mode.
    // Aspect ratio of the touch display's resolution during calibration.
    float calib_ar = touch_calib_size.width() / touch_calib_size.height();
    // Aspect ratio of the display that is being mirrored.
    float current_ar = current_size.width() / current_size.height();

    if (current_ar < calib_ar) {
      pre_transform.Scale(current_size.height() / touch_calib_size.height(),
                          current_size.height() / touch_calib_size.height());
      pre_transform.Translate(
          (current_ar / calib_ar - 1.f) * touch_calib_size.width() * 0.5f, 0);
    } else {
      pre_transform.Scale(current_size.width() / touch_calib_size.width(),
                          current_size.width() / touch_calib_size.width());
      pre_transform.Translate(
          0, (calib_ar / current_ar - 1.f) * touch_calib_size.height() * 0.5f);
    }
  } else {
    // Case of current resolution being different from the resolution when the
    // touch calibration was performed.
    pre_transform.Scale(current_size.width() / touch_calib_size.width(),
                        current_size.height() / touch_calib_size.height());
  }
  // Solve for coefficients and compute transform matrix.
  gfx::Transform stored_ctm;
  if (!GetCalibratedTransform(
          touch_display.GetTouchCalibrationData().point_pairs, pre_transform,
          &stored_ctm)) {
    // TODO(malaykeshav): This can be checked at the calibration step before
    // storing the calibration associated data. This will allow us to explicitly
    // inform the user with proper UX.

    // Clear stored calibration data.
    GetDisplayManager()->ClearTouchCalibrationData(touch_display.id());

    // Return uncalibrated transform.
    return GetUncalibratedTransform(ctm, display, touch_display, touch_area,
                                    touch_native_size);
  }

  stored_ctm.ConcatTransform(ctm);
  return stored_ctm;
}

TouchTransformerController::TouchTransformerController() {
  Shell::GetInstance()->window_tree_host_manager()->AddObserver(this);
}

TouchTransformerController::~TouchTransformerController() {
  Shell::GetInstance()->window_tree_host_manager()->RemoveObserver(this);
}

void TouchTransformerController::UpdateTouchRadius(
    const display::ManagedDisplayInfo& display) const {
  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();
  for (const auto& device_id : display.input_devices()) {
    device_manager->UpdateTouchRadiusScale(
        device_id,
        GetTouchResolutionScale(display, FindTouchscreenById(device_id)));
  }
}

void TouchTransformerController::UpdateTouchTransform(
    int64_t target_display_id,
    const display::ManagedDisplayInfo& touch_display,
    const display::ManagedDisplayInfo& target_display) const {
  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();
  gfx::Size fb_size =
      Shell::GetInstance()->display_configurator()->framebuffer_size();
  for (const auto& device_id : touch_display.input_devices()) {
    device_manager->UpdateTouchInfoForDisplay(
        target_display_id, device_id,
        GetTouchTransform(target_display, touch_display,
                          FindTouchscreenById(device_id), fb_size));
  }
}

void TouchTransformerController::UpdateTouchTransformer() const {
  ui::DeviceDataManager* device_manager = ui::DeviceDataManager::GetInstance();
  device_manager->ClearTouchDeviceAssociations();

  // Display IDs and display::ManagedDisplayInfo for mirror or extended mode.
  int64_t display1_id = display::kInvalidDisplayId;
  int64_t display2_id = display::kInvalidDisplayId;
  display::ManagedDisplayInfo display1;
  display::ManagedDisplayInfo display2;
  // Display ID and display::ManagedDisplayInfo for single display mode.
  int64_t single_display_id = display::kInvalidDisplayId;
  display::ManagedDisplayInfo single_display;

  WindowTreeHostManager* window_tree_host_manager =
      Shell::GetInstance()->window_tree_host_manager();
  display::DisplayManager* display_manager = GetDisplayManager();
  if (display_manager->num_connected_displays() == 0) {
    return;
  } else if (display_manager->num_connected_displays() == 1 ||
             display_manager->IsInUnifiedMode()) {
    single_display_id = display_manager->first_display_id();
    DCHECK(single_display_id != display::kInvalidDisplayId);
    single_display = display_manager->GetDisplayInfo(single_display_id);
    UpdateTouchRadius(single_display);
  } else {
    display::DisplayIdList list = display_manager->GetCurrentDisplayIdList();
    display1_id = list[0];
    display2_id = list[1];
    DCHECK(display1_id != display::kInvalidDisplayId &&
           display2_id != display::kInvalidDisplayId);
    display1 = display_manager->GetDisplayInfo(display1_id);
    display2 = display_manager->GetDisplayInfo(display2_id);
    UpdateTouchRadius(display1);
    UpdateTouchRadius(display2);
  }

  if (display_manager->IsInMirrorMode()) {
    int64_t primary_display_id =
        window_tree_host_manager->GetPrimaryDisplayId();
    if (GetDisplayManager()->SoftwareMirroringEnabled()) {
      // In extended but software mirroring mode, there is a WindowTreeHost for
      // each display, but all touches are forwarded to the primary root
      // window's WindowTreeHost.
      display::ManagedDisplayInfo target_display =
          primary_display_id == display1_id ? display1 : display2;
      UpdateTouchTransform(target_display.id(), display1, target_display);
      UpdateTouchTransform(target_display.id(), display2, target_display);
    } else {
      // In mirror mode, there is just one WindowTreeHost and two displays. Make
      // the WindowTreeHost accept touch events from both displays.
      UpdateTouchTransform(primary_display_id, display1, display1);
      UpdateTouchTransform(primary_display_id, display2, display2);
    }
    return;
  }

  if (display_manager->num_connected_displays() > 1) {
    // In actual extended mode, each display is associated with one
    // WindowTreeHost.
    UpdateTouchTransform(display1_id, display1, display1);
    UpdateTouchTransform(display2_id, display2, display2);
    return;
  }

  // Single display mode. The WindowTreeHost has one associated display id.
  UpdateTouchTransform(single_display_id, single_display, single_display);
}

void TouchTransformerController::OnDisplaysInitialized() {
  UpdateTouchTransformer();
}

void TouchTransformerController::OnDisplayConfigurationChanged() {
  UpdateTouchTransformer();
}

}  // namespace ash
