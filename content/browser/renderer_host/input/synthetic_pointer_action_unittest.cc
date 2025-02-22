// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_pointer_action.h"
#include "content/browser/renderer_host/input/synthetic_touch_driver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebMouseEvent;
using blink::WebTouchPoint;

namespace content {

namespace {

WebTouchPoint::State ToWebTouchPointState(
    SyntheticPointerActionParams::PointerActionType action_type) {
  switch (action_type) {
    case SyntheticPointerActionParams::PointerActionType::PRESS:
      return WebTouchPoint::StatePressed;
    case SyntheticPointerActionParams::PointerActionType::MOVE:
      return WebTouchPoint::StateMoved;
    case SyntheticPointerActionParams::PointerActionType::RELEASE:
      return WebTouchPoint::StateReleased;
    case SyntheticPointerActionParams::PointerActionType::IDLE:
      return WebTouchPoint::StateStationary;
    case SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED:
      NOTREACHED()
          << "Invalid SyntheticPointerActionParams::PointerActionType.";
      return WebTouchPoint::StateUndefined;
  }
  NOTREACHED() << "Invalid SyntheticPointerActionParams::PointerActionType.";
  return WebTouchPoint::StateUndefined;
}

WebInputEvent::Type ToWebMouseEventType(
    SyntheticPointerActionParams::PointerActionType action_type) {
  switch (action_type) {
    case SyntheticPointerActionParams::PointerActionType::PRESS:
      return WebInputEvent::MouseDown;
    case SyntheticPointerActionParams::PointerActionType::MOVE:
      return WebInputEvent::MouseMove;
    case SyntheticPointerActionParams::PointerActionType::RELEASE:
      return WebInputEvent::MouseUp;
    case SyntheticPointerActionParams::PointerActionType::IDLE:
    case SyntheticPointerActionParams::PointerActionType::NOT_INITIALIZED:
      NOTREACHED()
          << "Invalid SyntheticPointerActionParams::PointerActionType.";
      return WebInputEvent::Undefined;
  }
  NOTREACHED() << "Invalid SyntheticPointerActionParams::PointerActionType.";
  return WebInputEvent::Undefined;
}

class MockSyntheticPointerActionTarget : public SyntheticGestureTarget {
 public:
  MockSyntheticPointerActionTarget() {}
  ~MockSyntheticPointerActionTarget() override {}

  // SyntheticGestureTarget:
  void SetNeedsFlush() override { NOTIMPLEMENTED(); }

  base::TimeDelta PointerAssumedStoppedTime() const override {
    NOTIMPLEMENTED();
    return base::TimeDelta();
  }

  float GetTouchSlopInDips() const override {
    NOTIMPLEMENTED();
    return 0.0f;
  }

  float GetMinScalingSpanInDips() const override {
    NOTIMPLEMENTED();
    return 0.0f;
  }

  WebInputEvent::Type type() const { return type_; }

 protected:
  WebInputEvent::Type type_;
};

class MockSyntheticPointerTouchActionTarget
    : public MockSyntheticPointerActionTarget {
 public:
  MockSyntheticPointerTouchActionTarget() {}
  ~MockSyntheticPointerTouchActionTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    DCHECK(WebInputEvent::isTouchEventType(event.type));
    const WebTouchEvent& touch_event = static_cast<const WebTouchEvent&>(event);
    type_ = touch_event.type;
    for (size_t i = 0; i < WebTouchEvent::kTouchesLengthCap; ++i) {
      indexes_[i] = touch_event.touches[i].id;
      positions_[i] = gfx::PointF(touch_event.touches[i].position);
      states_[i] = touch_event.touches[i].state;
    }
    touch_length_ = touch_event.touchesLength;
  }

  testing::AssertionResult SyntheticTouchActionDispatchedCorrectly(
      const SyntheticPointerActionParams& param,
      int index) {
    if (param.pointer_action_type() ==
            SyntheticPointerActionParams::PointerActionType::PRESS ||
        param.pointer_action_type() ==
            SyntheticPointerActionParams::PointerActionType::MOVE) {
      if (indexes_[index] != param.index()) {
        return testing::AssertionFailure()
               << "Pointer index at index " << index << " was "
               << indexes_[index] << ", expected " << param.index() << ".";
      }

      if (positions_[index] != param.position()) {
        return testing::AssertionFailure()
               << "Pointer position at index " << index << " was "
               << positions_[index].ToString() << ", expected "
               << param.position().ToString() << ".";
      }
    }

    if (states_[index] != ToWebTouchPointState(param.pointer_action_type())) {
      return testing::AssertionFailure()
             << "Pointer states at index " << index << " was " << states_[index]
             << ", expected "
             << ToWebTouchPointState(param.pointer_action_type()) << ".";
    }
    return testing::AssertionSuccess();
  }

  testing::AssertionResult SyntheticTouchActionListDispatchedCorrectly(
      const std::vector<SyntheticPointerActionParams>& params_list) {
    if (touch_length_ != params_list.size()) {
      return testing::AssertionFailure() << "Touch point length was "
                                         << touch_length_ << ", expected "
                                         << params_list.size() << ".";
    }

    testing::AssertionResult result = testing::AssertionSuccess();
    for (size_t i = 0; i < params_list.size(); ++i) {
      result = SyntheticTouchActionDispatchedCorrectly(params_list[i],
                                                       params_list[i].index());
      if (result == testing::AssertionFailure())
        return result;
    }
    return testing::AssertionSuccess();
  }

  SyntheticGestureParams::GestureSourceType
  GetDefaultSyntheticGestureSourceType() const override {
    return SyntheticGestureParams::TOUCH_INPUT;
  }

 private:
  gfx::PointF positions_[WebTouchEvent::kTouchesLengthCap];
  unsigned touch_length_;
  int indexes_[WebTouchEvent::kTouchesLengthCap];
  WebTouchPoint::State states_[WebTouchEvent::kTouchesLengthCap];
};

class MockSyntheticPointerMouseActionTarget
    : public MockSyntheticPointerActionTarget {
 public:
  MockSyntheticPointerMouseActionTarget() {}
  ~MockSyntheticPointerMouseActionTarget() override {}

  void DispatchInputEventToPlatform(const WebInputEvent& event) override {
    DCHECK(WebInputEvent::isMouseEventType(event.type));
    const WebMouseEvent& mouse_event = static_cast<const WebMouseEvent&>(event);
    type_ = mouse_event.type;
    position_ = gfx::PointF(mouse_event.x, mouse_event.y);
    clickCount_ = mouse_event.clickCount;
    button_ = mouse_event.button;
  }

  testing::AssertionResult SyntheticMouseActionDispatchedCorrectly(
      const SyntheticPointerActionParams& param,
      int click_count) {
    if (type_ != ToWebMouseEventType(param.pointer_action_type())) {
      return testing::AssertionFailure()
             << "Pointer type was " << WebInputEvent::GetName(type_)
             << ", expected " << WebInputEvent::GetName(ToWebMouseEventType(
             param.pointer_action_type())) << ".";
    }

    if (clickCount_ != click_count) {
      return testing::AssertionFailure() << "Pointer click count was "
                                         << clickCount_ << ", expected "
                                         << click_count << ".";
    }

    if (clickCount_ == 1 && button_ != WebMouseEvent::Button::Left) {
      return testing::AssertionFailure()
             << "Pointer button was " << (int)button_ << ", expected "
             << (int)WebMouseEvent::Button::Left << ".";
    }

    if (clickCount_ == 0 && button_ != WebMouseEvent::Button::NoButton) {
      return testing::AssertionFailure()
             << "Pointer button was " << (int)button_ << ", expected "
             << (int)WebMouseEvent::Button::NoButton << ".";
    }

    if ((param.pointer_action_type() ==
             SyntheticPointerActionParams::PointerActionType::PRESS ||
         param.pointer_action_type() ==
             SyntheticPointerActionParams::PointerActionType::MOVE) &&
        position_ != param.position()) {
      return testing::AssertionFailure()
             << "Pointer position was " << position_.ToString()
             << ", expected " << param.position().ToString() << ".";
    }
    return testing::AssertionSuccess();
  }

  SyntheticGestureParams::GestureSourceType
  GetDefaultSyntheticGestureSourceType() const override {
    return SyntheticGestureParams::MOUSE_INPUT;
  }

 private:
  gfx::PointF position_;
  int clickCount_;
  WebMouseEvent::Button button_;
};

class SyntheticPointerActionTest : public testing::Test {
 public:
  SyntheticPointerActionTest() {
    params_ = SyntheticPointerActionListParams();
    num_success_ = 0;
    num_failure_ = 0;
  }
  ~SyntheticPointerActionTest() override {}

 protected:
  template <typename MockGestureTarget>
  void CreateSyntheticPointerActionTarget() {
    target_.reset(new MockGestureTarget());
    synthetic_pointer_driver_ = SyntheticPointerDriver::Create(
        target_->GetDefaultSyntheticGestureSourceType());
  }

  void ForwardSyntheticPointerAction() {
    SyntheticGesture::Result result = pointer_action_->ForwardInputEvents(
        base::TimeTicks::Now(), target_.get());

    if (result == SyntheticGesture::GESTURE_FINISHED ||
        result == SyntheticGesture::GESTURE_RUNNING)
      num_success_++;
    else
      num_failure_++;
  }

  int num_success_;
  int num_failure_;
  std::unique_ptr<MockSyntheticPointerActionTarget> target_;
  std::unique_ptr<SyntheticGesture> pointer_action_;
  std::unique_ptr<SyntheticPointerDriver> synthetic_pointer_driver_;
  SyntheticPointerActionListParams params_;
};

TEST_F(SyntheticPointerActionTest, PointerTouchAction) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerTouchActionTarget>();

  // Send a touch press for one finger.
  SyntheticPointerActionParams param1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param1.set_index(0);
  param1.set_position(gfx::PointF(54, 89));
  SyntheticPointerActionListParams::ParamList param_list1;
  param_list1.push_back(param1);
  params_.PushPointerActionParamsList(param_list1);

  // Send a touch move for the first finger and a touch press for the second
  // finger.
  param1.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param1.set_position(gfx::PointF(133, 156));
  SyntheticPointerActionParams param2 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param2.set_index(1);
  param2.set_position(gfx::PointF(79, 132));
  SyntheticPointerActionListParams::ParamList param_list2;
  param_list2.push_back(param1);
  param_list2.push_back(param2);
  params_.PushPointerActionParamsList(param_list2);

  // Send a touch move for the second finger.
  param1.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::IDLE);
  param2.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param2.set_position(gfx::PointF(87, 253));
  SyntheticPointerActionListParams::ParamList param_list3;
  param_list3.push_back(param1);
  param_list3.push_back(param2);
  params_.PushPointerActionParamsList(param_list3);

  // Send touch releases for both fingers.
  SyntheticPointerActionListParams::ParamList param_list4;
  param1.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  param2.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  param_list4.push_back(param1);
  param_list4.push_back(param2);
  params_.PushPointerActionParamsList(param_list4);
  pointer_action_.reset(new SyntheticPointerAction(params_));

  ForwardSyntheticPointerAction();
  MockSyntheticPointerTouchActionTarget* pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchStart);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      param_list1));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  // The type of the SyntheticWebTouchEvent is the action of the last finger.
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchStart);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      param_list2));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchMove);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      param_list3));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(4, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchEnd);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      param_list4));
}

TEST_F(SyntheticPointerActionTest, PointerTouchActionsMultiPressRelease) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerTouchActionTarget>();
  int count_success = 1;

  // Send a touch press for one finger.
  SyntheticPointerActionParams param1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param1.set_index(0);
  param1.set_position(gfx::PointF(54, 89));
  SyntheticPointerActionListParams::ParamList param_list1;
  param_list1.push_back(param1);
  params_.PushPointerActionParamsList(param_list1);

  SyntheticPointerActionParams param2 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param2.set_index(1);
  param2.set_position(gfx::PointF(123, 69));
  param1.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::IDLE);
  SyntheticPointerActionListParams::ParamList param_list2;
  param_list2.push_back(param1);
  param_list2.push_back(param2);

  param2.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  SyntheticPointerActionListParams::ParamList param_list3;
  param_list3.push_back(param1);
  param_list3.push_back(param2);
  for (int i = 0; i < 3; ++i) {
    // Send a touch press for the second finger and not move the first finger.
    params_.PushPointerActionParamsList(param_list2);

    // Send a touch release for the second finger and not move the first finger.
    params_.PushPointerActionParamsList(param_list3);
  }
  pointer_action_.reset(new SyntheticPointerAction(params_));

  ForwardSyntheticPointerAction();
  MockSyntheticPointerTouchActionTarget* pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_.get());
  EXPECT_EQ(count_success++, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchStart);
  EXPECT_TRUE(pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
      param_list1));

  for (int index = 1; index < 4; ++index) {
    ForwardSyntheticPointerAction();
    EXPECT_EQ(count_success++, num_success_);
    EXPECT_EQ(0, num_failure_);
    // The type of the SyntheticWebTouchEvent is the action of the last finger.
    EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchStart);
    EXPECT_TRUE(
        pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
            param_list2));

    ForwardSyntheticPointerAction();
    EXPECT_EQ(count_success++, num_success_);
    EXPECT_EQ(0, num_failure_);
    // The type of the SyntheticWebTouchEvent is the action of the last finger.
    EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchEnd);
    EXPECT_TRUE(
        pointer_touch_target->SyntheticTouchActionListDispatchedCorrectly(
            param_list3));
  }
}

TEST_F(SyntheticPointerActionTest, PointerTouchActionTypeInvalid) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerTouchActionTarget>();

  // Cannot send a touch move or touch release without sending a touch press
  // first.
  SyntheticPointerActionParams param = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param.set_index(0);
  param.set_position(gfx::PointF(54, 89));
  params_.PushPointerActionParams(param);
  pointer_action_.reset(new SyntheticPointerAction(params_));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(0, num_success_);
  EXPECT_EQ(1, num_failure_);

  param.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params_ = SyntheticPointerActionListParams();
  params_.PushPointerActionParams(param);
  pointer_action_.reset(new SyntheticPointerAction(params_));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(0, num_success_);
  EXPECT_EQ(2, num_failure_);

  // Send a touch press for one finger.
  param.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  params_ = SyntheticPointerActionListParams();
  params_.PushPointerActionParams(param);
  params_.PushPointerActionParams(param);
  pointer_action_.reset(new SyntheticPointerAction(params_));

  ForwardSyntheticPointerAction();
  MockSyntheticPointerTouchActionTarget* pointer_touch_target =
      static_cast<MockSyntheticPointerTouchActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(2, num_failure_);
  EXPECT_EQ(pointer_touch_target->type(), WebInputEvent::TouchStart);
  EXPECT_TRUE(
      pointer_touch_target->SyntheticTouchActionDispatchedCorrectly(param, 0));

  // Cannot send a touch press again without releasing the finger.
  ForwardSyntheticPointerAction();
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(3, num_failure_);
}

TEST_F(SyntheticPointerActionTest, PointerMouseAction) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerMouseActionTarget>();

  // Send a mouse move.
  SyntheticPointerActionParams param1 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param1.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param1);

  // Send a mouse down.
  SyntheticPointerActionParams param2 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param2.set_position(gfx::PointF(189, 62));
  params_.PushPointerActionParams(param2);

  // Send a mouse drag.
  SyntheticPointerActionParams param3 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::MOVE);
  param3.set_position(gfx::PointF(326, 298));
  params_.PushPointerActionParams(param3);

  // Send a mouse up.
  SyntheticPointerActionParams param4 = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params_.PushPointerActionParams(param4);
  pointer_action_.reset(new SyntheticPointerAction(params_));

  ForwardSyntheticPointerAction();
  MockSyntheticPointerMouseActionTarget* pointer_mouse_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(
      pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(param1, 0));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(2, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(
      pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(param2, 1));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(3, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(
      pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(param3, 1));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(4, num_success_);
  EXPECT_EQ(0, num_failure_);
  EXPECT_TRUE(
      pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(param4, 1));
}

TEST_F(SyntheticPointerActionTest, PointerMouseActionTypeInvalid) {
  CreateSyntheticPointerActionTarget<MockSyntheticPointerMouseActionTarget>();

  // Cannot send a mouse up without sending a mouse down first.
  SyntheticPointerActionParams param = SyntheticPointerActionParams(
      SyntheticPointerActionParams::PointerActionType::RELEASE);
  params_.PushPointerActionParams(param);
  pointer_action_.reset(new SyntheticPointerAction(params_));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(0, num_success_);
  EXPECT_EQ(1, num_failure_);

  // Send a mouse down for one finger.
  param.set_pointer_action_type(
      SyntheticPointerActionParams::PointerActionType::PRESS);
  param.set_position(gfx::PointF(54, 89));
  params_ = SyntheticPointerActionListParams();
  params_.PushPointerActionParams(param);

  // Cannot send a mouse down again without releasing the mouse button.
  params_.PushPointerActionParams(param);
  pointer_action_.reset(new SyntheticPointerAction(params_));

  ForwardSyntheticPointerAction();
  MockSyntheticPointerMouseActionTarget* pointer_mouse_target =
      static_cast<MockSyntheticPointerMouseActionTarget*>(target_.get());
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(1, num_failure_);
  EXPECT_TRUE(
      pointer_mouse_target->SyntheticMouseActionDispatchedCorrectly(param, 1));

  ForwardSyntheticPointerAction();
  EXPECT_EQ(1, num_success_);
  EXPECT_EQ(2, num_failure_);
}

}  // namespace

}  // namespace content