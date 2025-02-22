/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebAudioDevice_h
#define WebAudioDevice_h

#include "WebCommon.h"
#include "WebVector.h"

namespace blink {

// Abstract interface to the Chromium audio system.
class WebAudioDevice {
 public:
  class BLINK_PLATFORM_EXPORT RenderCallback {
   public:
    // Note: |delay| and |delayTimestamp| arguments are high-precision
    // measurements of the state of the system in the recent past. To be clear,
    // |delay| does *not* represent the point-in-time at which the first
    // rendered sample will be played out.
    virtual void render(const WebVector<float*>& destinationData,
                        size_t numberOfFrames,
                        double delay,           // Output delay in seconds.
                        double delayTimestamp,  // System timestamp in seconds
                                                // when |delay| was obtained.
                        size_t priorFramesSkipped);

   protected:
    virtual ~RenderCallback();
  };

  virtual ~WebAudioDevice() {}

  virtual void start() = 0;
  virtual void stop() = 0;
  virtual double sampleRate() = 0;
};

}  // namespace blink

#endif  // WebAudioDevice_h
