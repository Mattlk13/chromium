// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DISPLAY_TOUCH_CALIBRATOR_TOUCH_CALIBRATOR_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_DISPLAY_TOUCH_CALIBRATOR_TOUCH_CALIBRATOR_VIEW_H_

#include "base/macros.h"
#include "ui/display/display.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/view.h"

namespace views {
class Label;
class Widget;
}

namespace gfx {
class Animation;
class LinearAnimation;
}

namespace chromeos {

// An overlay view used during touch calibration. This view is responsible for
// all animations and UX during touch calibration on all displays currently
// active on the device. The view on the display being calibrated is the primary
// touch calibration view.
// |TouchCalibratorView| acts as a state machine and has an API to toggle its
// state or get the current state.
class TouchCalibratorView : public views::View, public gfx::AnimationDelegate {
 public:
  // Different states of |TouchCalibratorView| in order.
  enum State {
    UNKNOWN = 0,
    BACKGROUND_FADING_IN,  // Transition state where the background is fading
                           // in.
    DISPLAY_POINT_1,       // Static state where the touch point is at its first
                           // location.
    ANIMATING_1_TO_2,  // Transition state when the touch point is being moved
                       // from one location to another.
    DISPLAY_POINT_2,   // Static state where the touch point is at its second
                       // location.
    ANIMATING_2_TO_3,
    DISPLAY_POINT_3,  // Static state where the touch point is at its third
                      // location.
    ANIMATING_3_TO_4,
    DISPLAY_POINT_4,       // Static state where the touch point is at its final
                           // location.
    CALIBRATION_COMPLETE,  // Static state when the calibration complete message
                           // is displayed to the user.
    BACKGROUND_FADING_OUT  // Transition state where the background is fading
                           // out
  };

  TouchCalibratorView(const display::Display& target_display,
                      bool is_primary_view);
  ~TouchCalibratorView() override;

  // views::View overrides:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;

  // gfx::AnimationDelegate overrides:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

  // Moves the touch calibrator view to its next state.
  void AdvanceToNextState();

  // Skips to the final state. Should be used to cancel calibration and hide all
  // views from the screen with a smooth transition out animation.
  void SkipToFinalState();

  // Returns true if |location| is set by the end of this function call. If set,
  // |location| will point to the center of the circle that the user sees during
  // the touch calibration UX.
  bool GetDisplayPointLocation(gfx::Point* location);

  // Skips/cancels any ongoing animation to its end.
  void SkipCurrentAnimationForTest();

  // Returns the current state of the view.
  State state() { return state_; }

 private:
  void InitViewContents();

  // The target display on which this view is rendered on.
  const display::Display display_;

  // True if this view is on the display that is being calibrated.
  bool is_primary_view_ = false;

  std::unique_ptr<views::Widget> widget_;

  SkPaint paint_;

  // Defines the bounds for the background animation.
  gfx::RectF background_rect_;

  // Text label indicating how to exit the touch calibration.
  views::Label* exit_label_;

  // Start and end opacity values used during the fade animation. This is set
  // before the animation begins.
  float start_opacity_value_;
  float end_opacity_value_;

  // Linear animation used for various aniations including fade-in, fade out,
  // and view translation.
  std::unique_ptr<gfx::LinearAnimation> animator_;

  State state_ = UNKNOWN;

  DISALLOW_COPY_AND_ASSIGN(TouchCalibratorView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DISPLAY_TOUCH_CALIBRATOR_TOUCH_CALIBRATOR_VIEW_H_
