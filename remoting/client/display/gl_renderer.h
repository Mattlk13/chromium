// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_DISPLAY_GL_RENDERER_H_
#define REMOTING_CLIENT_DISPLAY_GL_RENDERER_H_

#include <queue>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/client/display/gl_cursor.h"
#include "remoting/client/display/gl_cursor_feedback.h"
#include "remoting/client/display/gl_desktop.h"
#include "remoting/proto/control.pb.h"

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {

namespace protocol {
class CursorShapeInfo;
}  // namespace protocol

class GlCanvas;
class GlRendererDelegate;
class GlRendererTest;

// Renders desktop and cursor on the OpenGL surface. Can be created on any
// thread but thereafter must be used and deleted on the same thread (usually
// the display thread. Or any Chromium thread with a task runner attached to
// it) unless otherwise noted.
// The unit of all length arguments is pixel.
class GlRenderer {
 public:
  explicit GlRenderer();
  ~GlRenderer();

  // The delegate can be set on any hread no more than once before calling any
  // On* functions.
  void SetDelegate(base::WeakPtr<GlRendererDelegate> delegate);

  // Notifies the delegate with the current canvas size. Canvas size will be
  // (0, 0) if no desktop frame is received yet.
  // Caller can use this function to get the canvas size when the surface is
  // recreated.
  void RequestCanvasSize();

  // Sets the pixel based transformation matrix related to the size of the
  // canvas.
  // 3 by 3 transformation matrix, [ m0, m1, m2, m3, m4, m5, m6, m7, m8 ].
  //
  // | m0, m1, m2, |   | x |
  // | m3, m4, m5, | * | y |
  // | m6, m7, m8  |   | 1 |
  //
  // The final size of the canvas will be (m0*canvas_width, m4*canvas_height)
  // and the top-left corner will be (m2, m5) in pixel coordinates.
  void OnPixelTransformationChanged(const std::array<float, 9>& matrix);

  void OnCursorMoved(float x, float y);

  void OnCursorInputFeedback(float x, float y, float diameter);

  void OnCursorVisibilityChanged(bool visible);

  // Called when a desktop frame is received.
  // The size of the canvas is determined by the dimension of the desktop frame.
  // |done| will be queued up and called on the display thread after the actual
  // rendering happens.
  void OnFrameReceived(std::unique_ptr<webrtc::DesktopFrame> frame,
                       const base::Closure& done);

  void OnCursorShapeChanged(const protocol::CursorShapeInfo& shape);

  // Called after the EGL/EAGL context is established and the surface is created
  // (or recreated). Previous desktop frame and canvas transformation will be
  // lost after calling this function.
  // Caller must call OnSurfaceDestroyed() before calling this function if the
  // surface is recreated.
  void OnSurfaceCreated(int gl_version);

  // Sets the size of the view. Called right after OnSurfaceCreated() or
  // whenever the view size is changed.
  void OnSurfaceChanged(int view_width, int view_height);

  // Called when the surface is destroyed.
  void OnSurfaceDestroyed();

  // Returns the weak pointer to be used on the display thread.
  base::WeakPtr<GlRenderer> GetWeakPtr();

 private:
  friend class GlRendererTest;

  // Post a rendering task to the task runner of current thread.
  // Do nothing if render_callback_ is not set yet or an existing rendering task
  // in the queue will cover changes before this function is called.
  void RequestRender();

  // Draws out everything on current OpenGL buffer and runs closures in
  // |pending_done_callbacks_|.
  // Nothing will be drawn nor the done callbacks will be run if |delegate_| is
  // invalid or !delegate_.CanRenderFrame().
  void OnRender();

  base::WeakPtr<GlRendererDelegate> delegate_;

  // Done callbacks from OnFrameReceived. Will all be called once rendering
  // takes place.
  std::queue<base::Closure> pending_done_callbacks_;

  bool render_scheduled_ = false;

  int canvas_width_ = 0;
  int canvas_height_ = 0;

  std::unique_ptr<GlCanvas> canvas_;

  GlCursor cursor_;
  GlCursorFeedback cursor_feedback_;
  GlDesktop desktop_;

  base::ThreadChecker thread_checker_;
  base::WeakPtr<GlRenderer> weak_ptr_;
  base::WeakPtrFactory<GlRenderer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GlRenderer);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_DISPLAY_GL_RENDERER_H_
