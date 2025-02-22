/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef AudioScheduledSourceNode_h
#define AudioScheduledSourceNode_h

#include "bindings/core/v8/ActiveScriptWrappable.h"
#include "modules/webaudio/AudioNode.h"

namespace blink {

class BaseAudioContext;
class AudioBus;

class AudioScheduledSourceHandler : public AudioHandler {
 public:
  // These are the possible states an AudioScheduledSourceNode can be in:
  //
  // UNSCHEDULED_STATE - Initial playback state. Created, but not yet scheduled.
  // SCHEDULED_STATE - Scheduled to play (via start()), but not yet playing.
  // PLAYING_STATE - Generating sound.
  // FINISHED_STATE - Finished generating sound.
  //
  // The state can only transition to the next state, except for the
  // FINISHED_STATE which can never be changed.
  enum PlaybackState {
    // These must be defined with the same names and values as in the .idl file.
    UNSCHEDULED_STATE = 0,
    SCHEDULED_STATE = 1,
    PLAYING_STATE = 2,
    FINISHED_STATE = 3
  };

  AudioScheduledSourceHandler(NodeType, AudioNode&, float sampleRate);

  // Scheduling.
  void start(double when, ExceptionState&);
  void stop(double when, ExceptionState&);

  PlaybackState playbackState() const {
    return static_cast<PlaybackState>(acquireLoad(&m_playbackState));
  }

  void setPlaybackState(PlaybackState newState) {
    releaseStore(&m_playbackState, newState);
  }

  bool isPlayingOrScheduled() const {
    PlaybackState state = playbackState();
    return state == PLAYING_STATE || state == SCHEDULED_STATE;
  }

  bool hasFinished() const { return playbackState() == FINISHED_STATE; }

 protected:
  // Get frame information for the current time quantum.
  // We handle the transition into PLAYING_STATE and FINISHED_STATE here,
  // zeroing out portions of the outputBus which are outside the range of
  // startFrame and endFrame.
  //
  // Each frame time is relative to the context's currentSampleFrame().
  // quantumFrameOffset    : Offset frame in this time quantum to start
  //                         rendering.
  // nonSilentFramesToProcess : Number of frames rendering non-silence (will be
  //                            <= quantumFrameSize).
  void updateSchedulingInfo(size_t quantumFrameSize,
                            AudioBus* outputBus,
                            size_t& quantumFrameOffset,
                            size_t& nonSilentFramesToProcess);

  // Called when we have no more sound to play or the stop() time has been
  // reached. No onEnded event is called.
  virtual void finishWithoutOnEnded();

  // Like finishWithoutOnEnded(), but an onEnded (if specified) is called.
  virtual void finish();

  void notifyEnded();

  // This synchronizes with process() and any other method that needs to be
  // synchronized like setBuffer for AudioBufferSource.
  mutable Mutex m_processLock;

  // m_startTime is the time to start playing based on the context's timeline (0
  // or a time less than the context's current time means "now").
  double m_startTime;  // in seconds

  // m_endTime is the time to stop playing based on the context's timeline (0 or
  // a time less than the context's current time means "now").  If it hasn't
  // been set explicitly, then the sound will not stop playing (if looping) or
  // will stop when the end of the AudioBuffer has been reached.
  double m_endTime;  // in seconds

  static const double UnknownTime;

 private:
  // This is accessed by both the main thread and audio thread.  Use the setter
  // and getter to protect the access to this.
  int m_playbackState;
};

class AudioScheduledSourceNode
    : public AudioNode,
      public ActiveScriptWrappable<AudioScheduledSourceNode> {
  USING_GARBAGE_COLLECTED_MIXIN(AudioScheduledSourceNode);
  DEFINE_WRAPPERTYPEINFO();

 public:
  void start(ExceptionState&);
  void start(double when, ExceptionState&);
  void stop(ExceptionState&);
  void stop(double when, ExceptionState&);

  EventListener* onended();
  void setOnended(EventListener*);

  // ScriptWrappable:
  bool hasPendingActivity() const final;

  DEFINE_INLINE_VIRTUAL_TRACE() { AudioNode::trace(visitor); }

 protected:
  explicit AudioScheduledSourceNode(BaseAudioContext&);
  AudioScheduledSourceHandler& audioScheduledSourceHandler() const;
};

}  // namespace blink

#endif  // AudioScheduledSourceNode_h
