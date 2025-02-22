// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_FRAME_RECEIVER_ON_IO_THREAD_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_FRAME_RECEIVER_ON_IO_THREAD_H_

#include "content/common/content_export.h"
#include "media/capture/video/video_frame_receiver.h"

namespace content {

// Decorator for media::VideoFrameReceiver that forwards all incoming calls
// to the Browser IO thread.
// TODO(chfremer): Change this to VideoFrameReceiverOnTaskRunner and have the
// target task runner be passed into the constructor. See crbug.com/674190.
class CONTENT_EXPORT VideoFrameReceiverOnIOThread
    : public media::VideoFrameReceiver {
 public:
  explicit VideoFrameReceiverOnIOThread(
      const base::WeakPtr<VideoFrameReceiver>& receiver);
  ~VideoFrameReceiverOnIOThread() override;

  void OnIncomingCapturedVideoFrame(
      std::unique_ptr<media::VideoCaptureDevice::Client::Buffer> buffer,
      scoped_refptr<media::VideoFrame> frame) override;
  void OnError() override;
  void OnLog(const std::string& message) override;
  void OnBufferDestroyed(int buffer_id_to_drop) override;

 private:
  base::WeakPtr<VideoFrameReceiver> receiver_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_FRAME_RECEIVER_ON_IO_THREAD_H_
