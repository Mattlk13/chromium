// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget.h"

#include <tuple>
#include <vector>

#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/common/resize_params.h"
#include "content/public/test/mock_render_thread.h"
#include "content/renderer/devtools/render_widget_screen_metrics_emulator.h"
#include "content/test/fake_compositor_dependencies.h"
#include "content/test/mock_render_process.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebDeviceEmulationParams.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/gfx/geometry/rect.h"

using testing::_;

namespace content {

namespace {

const char* EVENT_LISTENER_RESULT_HISTOGRAM = "Event.PassiveListeners";

// Keep in sync with enum defined in
// RenderWidgetInputHandler::LogPassiveEventListenersUma.
enum {
  PASSIVE_LISTENER_UMA_ENUM_PASSIVE,
  PASSIVE_LISTENER_UMA_ENUM_UNCANCELABLE,
  PASSIVE_LISTENER_UMA_ENUM_SUPPRESSED,
  PASSIVE_LISTENER_UMA_ENUM_CANCELABLE,
  PASSIVE_LISTENER_UMA_ENUM_CANCELABLE_AND_CANCELED,
  PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING,
  PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_MAIN_THREAD_RESPONSIVENESS,
  PASSIVE_LISTENER_UMA_ENUM_COUNT
};

class MockWebWidget : public blink::WebWidget {
 public:
  MOCK_METHOD1(handleInputEvent,
               blink::WebInputEventResult(const blink::WebInputEvent&));
};

}  // namespace

class InteractiveRenderWidget : public RenderWidget {
 public:
  explicit InteractiveRenderWidget(CompositorDependencies* compositor_deps)
      : RenderWidget(++next_routing_id_,
                     compositor_deps,
                     blink::WebPopupTypeNone,
                     ScreenInfo(),
                     false,
                     false,
                     false),
        always_overscroll_(false) {
    Init(RenderWidget::ShowCallback(), mock_webwidget());
  }

  void SetTouchRegion(const std::vector<gfx::Rect>& rects) {
    rects_ = rects;
  }

  void SendInputEvent(const blink::WebInputEvent& event) {
    OnHandleInputEvent(
        &event, ui::LatencyInfo(),
        ui::WebInputEventTraits::ShouldBlockEventStream(event)
            ? InputEventDispatchType::DISPATCH_TYPE_BLOCKING
            : InputEventDispatchType::DISPATCH_TYPE_NON_BLOCKING);
  }

  void set_always_overscroll(bool overscroll) {
    always_overscroll_ = overscroll;
  }

  IPC::TestSink* sink() { return &sink_; }

  MockWebWidget* mock_webwidget() { return &mock_webwidget_; }

 protected:
  ~InteractiveRenderWidget() override { webwidget_internal_ = nullptr; }

  // Overridden from RenderWidget:
  bool HasTouchEventHandlersAt(const gfx::Point& point) const override {
    for (std::vector<gfx::Rect>::const_iterator iter = rects_.begin();
         iter != rects_.end(); ++iter) {
      if ((*iter).Contains(point))
        return true;
    }
    return false;
  }

  bool WillHandleGestureEvent(const blink::WebGestureEvent& event) override {
    if (always_overscroll_ &&
        event.type == blink::WebInputEvent::GestureScrollUpdate) {
      didOverscroll(blink::WebFloatSize(event.data.scrollUpdate.deltaX,
                                        event.data.scrollUpdate.deltaY),
                    blink::WebFloatSize(event.data.scrollUpdate.deltaX,
                                        event.data.scrollUpdate.deltaY),
                    blink::WebFloatPoint(event.x, event.y),
                    blink::WebFloatSize(event.data.scrollUpdate.velocityX,
                                        event.data.scrollUpdate.velocityY));
      return true;
    }

    return false;
  }

  bool Send(IPC::Message* msg) override {
    sink_.OnMessageReceived(*msg);
    delete msg;
    return true;
  }

 private:
  std::vector<gfx::Rect> rects_;
  IPC::TestSink sink_;
  bool always_overscroll_;
  MockWebWidget mock_webwidget_;
  static int next_routing_id_;

  DISALLOW_COPY_AND_ASSIGN(InteractiveRenderWidget);
};

int InteractiveRenderWidget::next_routing_id_ = 0;

class RenderWidgetUnittest : public testing::Test {
 public:
  RenderWidgetUnittest()
      : widget_(new InteractiveRenderWidget(&compositor_deps_)) {
    // RenderWidget::Init does an AddRef that's balanced by a browser-initiated
    // Close IPC. That Close will never happen in this test, so do a Release
    // here to ensure |widget_| is properly freed.
    widget_->Release();
    DCHECK(widget_->HasOneRef());
  }
  ~RenderWidgetUnittest() override {}

  InteractiveRenderWidget* widget() const { return widget_.get(); }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 private:
  MockRenderProcess render_process_;
  MockRenderThread render_thread_;
  FakeCompositorDependencies compositor_deps_;
  scoped_refptr<InteractiveRenderWidget> widget_;
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetUnittest);
};

TEST_F(RenderWidgetUnittest, TouchHitTestSinglePoint) {
  SyntheticWebTouchEvent touch;
  touch.PressPoint(10, 10);

  EXPECT_CALL(*widget()->mock_webwidget(), handleInputEvent(_))
      .WillRepeatedly(
          ::testing::Return(blink::WebInputEventResult::NotHandled));

  widget()->SendInputEvent(touch);
  ASSERT_EQ(1u, widget()->sink()->message_count());

  // Since there's currently no touch-event handling region, the response should
  // be 'no consumer exists'.
  const IPC::Message* message = widget()->sink()->GetMessageAt(0);
  EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Param params;
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  InputEventAckState ack_state = std::get<0>(params).state;
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, ack_state);
  widget()->sink()->ClearMessages();

  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(0, 0, 20, 20));
  rects.push_back(gfx::Rect(25, 0, 10, 10));
  widget()->SetTouchRegion(rects);

  EXPECT_CALL(*widget()->mock_webwidget(), handleInputEvent(_))
      .WillRepeatedly(
          ::testing::Return(blink::WebInputEventResult::NotHandled));

  widget()->SendInputEvent(touch);
  ASSERT_EQ(1u, widget()->sink()->message_count());
  message = widget()->sink()->GetMessageAt(0);
  EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  ack_state = std::get<0>(params).state;
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, ack_state);
  widget()->sink()->ClearMessages();
}

TEST_F(RenderWidgetUnittest, TouchHitTestMultiplePoints) {
  std::vector<gfx::Rect> rects;
  rects.push_back(gfx::Rect(0, 0, 20, 20));
  rects.push_back(gfx::Rect(25, 0, 10, 10));
  widget()->SetTouchRegion(rects);

  SyntheticWebTouchEvent touch;
  touch.PressPoint(25, 25);

  EXPECT_CALL(*widget()->mock_webwidget(), handleInputEvent(_))
      .WillRepeatedly(
          ::testing::Return(blink::WebInputEventResult::NotHandled));

  widget()->SendInputEvent(touch);
  ASSERT_EQ(1u, widget()->sink()->message_count());

  // Since there's currently no touch-event handling region, the response should
  // be 'no consumer exists'.
  const IPC::Message* message = widget()->sink()->GetMessageAt(0);
  EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Param params;
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  InputEventAckState ack_state = std::get<0>(params).state;
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS, ack_state);
  widget()->sink()->ClearMessages();

  // Press a second touch point. This time, on a touch-handling region.
  touch.PressPoint(10, 10);
  widget()->SendInputEvent(touch);
  ASSERT_EQ(1u, widget()->sink()->message_count());
  message = widget()->sink()->GetMessageAt(0);
  EXPECT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  ack_state = std::get<0>(params).state;
  EXPECT_EQ(INPUT_EVENT_ACK_STATE_NOT_CONSUMED, ack_state);
  widget()->sink()->ClearMessages();
}

TEST_F(RenderWidgetUnittest, EventOverscroll) {
  widget()->set_always_overscroll(true);

  EXPECT_CALL(*widget()->mock_webwidget(), handleInputEvent(_))
      .WillRepeatedly(
          ::testing::Return(blink::WebInputEventResult::NotHandled));

  blink::WebGestureEvent scroll(
      blink::WebInputEvent::GestureScrollUpdate,
      blink::WebInputEvent::NoModifiers,
      ui::EventTimeStampToSeconds(ui::EventTimeForNow()));
  scroll.x = -10;
  scroll.data.scrollUpdate.deltaY = 10;
  widget()->SendInputEvent(scroll);

  // Overscroll notifications received while handling an input event should
  // be bundled with the event ack IPC.
  ASSERT_EQ(1u, widget()->sink()->message_count());
  const IPC::Message* message = widget()->sink()->GetMessageAt(0);
  ASSERT_EQ(InputHostMsg_HandleInputEvent_ACK::ID, message->type());
  InputHostMsg_HandleInputEvent_ACK::Param params;
  InputHostMsg_HandleInputEvent_ACK::Read(message, &params);
  const InputEventAck& ack = std::get<0>(params);
  ASSERT_EQ(ack.type, scroll.type);
  ASSERT_TRUE(ack.overscroll);
  EXPECT_EQ(gfx::Vector2dF(0, 10), ack.overscroll->accumulated_overscroll);
  EXPECT_EQ(gfx::Vector2dF(0, 10), ack.overscroll->latest_overscroll_delta);
  EXPECT_EQ(gfx::Vector2dF(), ack.overscroll->current_fling_velocity);
  EXPECT_EQ(gfx::PointF(-10, 0), ack.overscroll->causal_event_viewport_point);
  widget()->sink()->ClearMessages();
}

TEST_F(RenderWidgetUnittest, FlingOverscroll) {
  // Overscroll notifications received outside of handling an input event should
  // be sent as a separate IPC.
  widget()->didOverscroll(blink::WebFloatSize(10, 5), blink::WebFloatSize(5, 5),
                          blink::WebFloatPoint(1, 1),
                          blink::WebFloatSize(10, 5));
  ASSERT_EQ(1u, widget()->sink()->message_count());
  const IPC::Message* message = widget()->sink()->GetMessageAt(0);
  ASSERT_EQ(InputHostMsg_DidOverscroll::ID, message->type());
  InputHostMsg_DidOverscroll::Param params;
  InputHostMsg_DidOverscroll::Read(message, &params);
  const ui::DidOverscrollParams& overscroll = std::get<0>(params);
  EXPECT_EQ(gfx::Vector2dF(10, 5), overscroll.latest_overscroll_delta);
  EXPECT_EQ(gfx::Vector2dF(5, 5), overscroll.accumulated_overscroll);
  EXPECT_EQ(gfx::PointF(1, 1), overscroll.causal_event_viewport_point);
  EXPECT_EQ(gfx::Vector2dF(10, 5), overscroll.current_fling_velocity);
  widget()->sink()->ClearMessages();
}

TEST_F(RenderWidgetUnittest, RenderWidgetInputEventUmaMetrics) {
  SyntheticWebTouchEvent touch;
  touch.PressPoint(10, 10);
  touch.touchStartOrFirstTouchMove = true;

  EXPECT_CALL(*widget()->mock_webwidget(), handleInputEvent(_))
      .Times(7)
      .WillRepeatedly(
          ::testing::Return(blink::WebInputEventResult::NotHandled));

  widget()->SendInputEvent(touch);
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_CANCELABLE, 1);

  touch.dispatchType = blink::WebInputEvent::DispatchType::EventNonBlocking;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_UNCANCELABLE,
                                       1);

  touch.dispatchType =
      blink::WebInputEvent::DispatchType::ListenersNonBlockingPassive;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_PASSIVE, 1);

  touch.dispatchType =
      blink::WebInputEvent::DispatchType::ListenersForcedNonBlockingDueToFling;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING, 1);

  touch.MovePoint(0, 10, 10);
  touch.touchStartOrFirstTouchMove = true;
  touch.dispatchType =
      blink::WebInputEvent::DispatchType::ListenersForcedNonBlockingDueToFling;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_FLING, 2);

  touch.dispatchType = blink::WebInputEvent::DispatchType::
      ListenersForcedNonBlockingDueToMainThreadResponsiveness;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_MAIN_THREAD_RESPONSIVENESS,
      1);

  touch.MovePoint(0, 10, 10);
  touch.touchStartOrFirstTouchMove = true;
  touch.dispatchType = blink::WebInputEvent::DispatchType::
      ListenersForcedNonBlockingDueToMainThreadResponsiveness;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_FORCED_NON_BLOCKING_DUE_TO_MAIN_THREAD_RESPONSIVENESS,
      2);

  EXPECT_CALL(*widget()->mock_webwidget(), handleInputEvent(_))
      .WillOnce(
          ::testing::Return(blink::WebInputEventResult::HandledSuppressed));
  touch.dispatchType = blink::WebInputEvent::DispatchType::Blocking;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectBucketCount(EVENT_LISTENER_RESULT_HISTOGRAM,
                                       PASSIVE_LISTENER_UMA_ENUM_SUPPRESSED, 1);

  EXPECT_CALL(*widget()->mock_webwidget(), handleInputEvent(_))
      .WillOnce(
          ::testing::Return(blink::WebInputEventResult::HandledApplication));
  touch.dispatchType = blink::WebInputEvent::DispatchType::Blocking;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectBucketCount(
      EVENT_LISTENER_RESULT_HISTOGRAM,
      PASSIVE_LISTENER_UMA_ENUM_CANCELABLE_AND_CANCELED, 1);
}

TEST_F(RenderWidgetUnittest, TouchDuringOrOutsideFlingUmaMetrics) {
  EXPECT_CALL(*widget()->mock_webwidget(), handleInputEvent(_))
      .Times(3)
      .WillRepeatedly(
          ::testing::Return(blink::WebInputEventResult::NotHandled));

  SyntheticWebTouchEvent touch;
  touch.PressPoint(10, 10);
  touch.dispatchType = blink::WebInputEvent::DispatchType::Blocking;
  touch.touchStartOrFirstTouchMove = true;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectTotalCount("Event.Touch.TouchLatencyOutsideFling",
                                      1);

  touch.MovePoint(0, 10, 10);
  touch.touchStartOrFirstTouchMove = true;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectTotalCount("Event.Touch.TouchLatencyOutsideFling",
                                      2);

  touch.MovePoint(0, 30, 30);
  touch.touchStartOrFirstTouchMove = false;
  widget()->SendInputEvent(touch);
  histogram_tester().ExpectTotalCount("Event.Touch.TouchLatencyOutsideFling",
                                      2);
}

class PopupRenderWidget : public RenderWidget {
 public:
  explicit PopupRenderWidget(CompositorDependencies* compositor_deps)
      : RenderWidget(1,
                     compositor_deps,
                     blink::WebPopupTypePage,
                     ScreenInfo(),
                     false,
                     false,
                     false) {
    Init(RenderWidget::ShowCallback(), mock_webwidget());
    did_show_ = true;
  }

  IPC::TestSink* sink() { return &sink_; }

  MockWebWidget* mock_webwidget() { return &mock_webwidget_; }

  void SetScreenMetricsEmulationParameters(
      bool,
      const blink::WebDeviceEmulationParams&) override {}

 protected:
  ~PopupRenderWidget() override { webwidget_internal_ = nullptr; }

  bool Send(IPC::Message* msg) override {
    sink_.OnMessageReceived(*msg);
    delete msg;
    return true;
  }

 private:
  IPC::TestSink sink_;
  MockWebWidget mock_webwidget_;

  DISALLOW_COPY_AND_ASSIGN(PopupRenderWidget);
};

class RenderWidgetPopupUnittest : public testing::Test {
 public:
  RenderWidgetPopupUnittest()
      : widget_(new PopupRenderWidget(&compositor_deps_)) {
    // RenderWidget::Init does an AddRef that's balanced by a browser-initiated
    // Close IPC. That Close will never happen in this test, so do a Release
    // here to ensure |widget_| is properly freed.
    widget_->Release();
    DCHECK(widget_->HasOneRef());
  }
  ~RenderWidgetPopupUnittest() override {}

  PopupRenderWidget* widget() const { return widget_.get(); }
  FakeCompositorDependencies compositor_deps_;

 private:
  MockRenderProcess render_process_;
  MockRenderThread render_thread_;
  scoped_refptr<PopupRenderWidget> widget_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetPopupUnittest);
};

TEST_F(RenderWidgetPopupUnittest, EmulatingPopupRect) {
  blink::WebRect popup_screen_rect(200, 250, 100, 400);
  widget()->setWindowRect(popup_screen_rect);

  // The view and window rect on a popup type RenderWidget should be
  // immediately set, without requiring an ACK.
  EXPECT_EQ(popup_screen_rect.x, widget()->windowRect().x);
  EXPECT_EQ(popup_screen_rect.y, widget()->windowRect().y);

  EXPECT_EQ(popup_screen_rect.x, widget()->viewRect().x);
  EXPECT_EQ(popup_screen_rect.y, widget()->viewRect().y);

  gfx::Rect emulated_window_rect(0, 0, 980, 1200);

  blink::WebDeviceEmulationParams emulation_params;
  emulation_params.screenPosition = blink::WebDeviceEmulationParams::Mobile;
  emulation_params.viewSize = emulated_window_rect.size();
  emulation_params.viewPosition = blink::WebPoint(150, 160);
  emulation_params.fitToView = true;

  gfx::Rect parent_window_rect = gfx::Rect(0, 0, 800, 600);

  ResizeParams resize_params;
  resize_params.new_size = parent_window_rect.size();

  scoped_refptr<PopupRenderWidget> parent_widget(
      new PopupRenderWidget(&compositor_deps_));
  parent_widget->Release();  // Balance Init().
  RenderWidgetScreenMetricsEmulator emulator(
      parent_widget.get(), emulation_params, resize_params, parent_window_rect,
      parent_window_rect);
  emulator.Apply();

  widget()->SetPopupOriginAdjustmentsForEmulation(&emulator);

  // Emulation-applied scale factor to fit the emulated device in the window.
  float scale =
      (float)parent_window_rect.height() / emulated_window_rect.height();

  // Used to center the emulated device in the window.
  gfx::Point offset(
      (parent_window_rect.width() - emulated_window_rect.width() * scale) / 2,
      (parent_window_rect.height() - emulated_window_rect.height() * scale) /
          2);

  // Position of the popup as seen by the emulated widget.
  gfx::Point emulated_position(emulation_params.viewPosition.x +
                                   (popup_screen_rect.x - offset.x()) / scale,
                               emulation_params.viewPosition.y +
                                   (popup_screen_rect.y - offset.y()) / scale);

  // Both the window and view rects as read from the accessors should have the
  // emulation parameters applied.
  EXPECT_EQ(emulated_position.x(), widget()->windowRect().x);
  EXPECT_EQ(emulated_position.y(), widget()->windowRect().y);
  EXPECT_EQ(emulated_position.x(), widget()->viewRect().x);
  EXPECT_EQ(emulated_position.y(), widget()->viewRect().y);

  // Setting a new window rect while emulated should remove the emulation
  // transformation from the given rect so that getting the rect, which applies
  // the transformation to the raw rect, should result in the same value.
  blink::WebRect popup_emulated_rect(130, 170, 100, 400);
  widget()->setWindowRect(popup_emulated_rect);

  EXPECT_EQ(popup_emulated_rect.x, widget()->windowRect().x);
  EXPECT_EQ(popup_emulated_rect.y, widget()->windowRect().y);
  EXPECT_EQ(popup_emulated_rect.x, widget()->viewRect().x);
  EXPECT_EQ(popup_emulated_rect.y, widget()->viewRect().y);
}

}  // namespace content
