// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/touchscreen_tap_suppression_controller.h"

#include <utility>

#include "content/browser/renderer_host/input/gesture_event_queue.h"

using blink::WebInputEvent;

namespace content {

TouchscreenTapSuppressionController::TouchscreenTapSuppressionController(
    GestureEventQueue* geq,
    const TapSuppressionController::Config& config)
    : gesture_event_queue_(geq), controller_(this, config) {
}

TouchscreenTapSuppressionController::~TouchscreenTapSuppressionController() {}

void TouchscreenTapSuppressionController::GestureFlingCancel() {
  controller_.GestureFlingCancel();
}

void TouchscreenTapSuppressionController::GestureFlingCancelAck(
    bool processed) {
  controller_.GestureFlingCancelAck(processed);
}

bool TouchscreenTapSuppressionController::FilterTapEvent(
    const GestureEventWithLatencyInfo& event) {
  switch (event.event.type) {
    case WebInputEvent::GestureTapDown:
      forward_next_tap_cancel_ = false;
      if (!controller_.ShouldDeferTapDown())
        return false;
      stashed_tap_down_.reset(new GestureEventWithLatencyInfo(event));
      return true;

    case WebInputEvent::GestureShowPress:
      if (!stashed_tap_down_)
        return false;
      stashed_show_press_.reset(new GestureEventWithLatencyInfo(event));
      return true;

    case WebInputEvent::GestureLongPress:
      // It is possible that a GestureLongPress arrives after tapDownTimer
      // expiration, in this case it should still get filtered if the
      // controller suppresses the tap end events.
      if (!stashed_tap_down_)
        return controller_.ShouldSuppressTapEnd();

      stashed_long_press_.reset(new GestureEventWithLatencyInfo(event));
      return true;

    case WebInputEvent::GestureTapUnconfirmed:
      return !!stashed_tap_down_;

    case WebInputEvent::GestureTapCancel:
      return !forward_next_tap_cancel_ && controller_.ShouldSuppressTapEnd();

    case WebInputEvent::GestureTap:
    case WebInputEvent::GestureDoubleTap:
    case WebInputEvent::GestureLongTap:
    case WebInputEvent::GestureTwoFingerTap:
      return controller_.ShouldSuppressTapEnd();

    default:
      break;
  }
  return false;
}

void TouchscreenTapSuppressionController::DropStashedTapDown() {
  stashed_tap_down_.reset();
  stashed_show_press_.reset();
  stashed_long_press_.reset();
}

void TouchscreenTapSuppressionController::ForwardStashedGestureEvents() {
  DCHECK(stashed_tap_down_);
  ScopedGestureEvent tap_down = std::move(stashed_tap_down_);
  ScopedGestureEvent show_press = std::move(stashed_show_press_);
  ScopedGestureEvent long_press = std::move(stashed_long_press_);
  gesture_event_queue_->ForwardGestureEvent(*tap_down);
  if (show_press)
    gesture_event_queue_->ForwardGestureEvent(*show_press);
  if (long_press)
    gesture_event_queue_->ForwardGestureEvent(*long_press);
}

void TouchscreenTapSuppressionController::ForwardStashedTapDown() {
  DCHECK(stashed_tap_down_);
  ScopedGestureEvent tap_down = std::move(stashed_tap_down_);
  gesture_event_queue_->ForwardGestureEvent(*tap_down);
  stashed_show_press_.reset();
  stashed_long_press_.reset();
  forward_next_tap_cancel_ = true;
}

}  // namespace content
