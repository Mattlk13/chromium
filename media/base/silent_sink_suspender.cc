// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/silent_sink_suspender.h"

#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace media {

SilentSinkSuspender::SilentSinkSuspender(
    AudioRendererSink::RenderCallback* callback,
    base::TimeDelta silence_timeout,
    const AudioParameters& params,
    const scoped_refptr<AudioRendererSink>& sink,
    const scoped_refptr<base::SingleThreadTaskRunner>& worker)
    : callback_(callback),
      params_(params),
      sink_(sink),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      silence_timeout_(silence_timeout),
      fake_sink_(worker, params_),
      sink_transition_callback_(
          base::Bind(&SilentSinkSuspender::TransitionSinks,
                     base::Unretained(this))) {
  DCHECK(params_.IsValid());
  DCHECK(sink_);
  DCHECK(callback_);
  DCHECK(task_runner_->BelongsToCurrentThread());
}

SilentSinkSuspender::~SilentSinkSuspender() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  fake_sink_.Stop();
}

int SilentSinkSuspender::Render(base::TimeDelta delay,
                                base::TimeTicks delay_timestamp,
                                int prior_frames_skipped,
                                AudioBus* dest) {
  // Lock required since AudioRendererSink::Pause() is not synchronous, we need
  // to discard these calls during the transition to the fake sink.
  base::AutoLock al(transition_lock_);
  if (is_using_fake_sink_ && dest) {
    // Audio should be silent at this point, if not, it will be handled once the
    // transition to the fake sink is complete.
    dest->Zero();
    return dest->frames();
  }

  // When we're using the |fake_sink_| a null destination will be sent; we store
  // the audio data for a future transition out of silence.
  if (!dest) {
    DCHECK(is_using_fake_sink_);
    DCHECK_EQ(delay, base::TimeDelta());
    DCHECK_EQ(prior_frames_skipped, 0);

    // If we have no buffers or a transition is pending, one or more extra
    // Render() calls have occurred in before TransitionSinks() can run, so we
    // store this data for the eventual transition.
    if (buffers_after_silence_.empty() || is_transition_pending_)
      buffers_after_silence_.push_back(AudioBus::Create(params_));
    dest = buffers_after_silence_.back().get();
  } else if (!buffers_after_silence_.empty()) {
    // Drain any non-silent transitional buffers before queuing more audio data.
    // Note: These do not skew clocks derived from frame count since we don't
    // issue Render() to the client when returning these buffers.
    DCHECK(!is_using_fake_sink_);
    buffers_after_silence_.front()->CopyTo(dest);
    buffers_after_silence_.pop_front();
    return dest->frames();
  }

  // Pass-through to client and request rendering.
  callback_->Render(delay, delay_timestamp, prior_frames_skipped, dest);

  // Check for silence or real audio data and transition if necessary.
  if (!dest->AreFramesZero()) {
    first_silence_time_ = base::TimeTicks();
    if (is_using_fake_sink_) {
      is_transition_pending_ = true;
      task_runner_->PostTask(
          FROM_HERE, base::Bind(sink_transition_callback_.callback(), false));
      return dest->frames();
    }
  } else if (!is_using_fake_sink_) {
    const base::TimeTicks now = base::TimeTicks::Now();
    if (first_silence_time_.is_null())
      first_silence_time_ = now;
    if (now - first_silence_time_ > silence_timeout_) {
      is_transition_pending_ = true;
      task_runner_->PostTask(
          FROM_HERE, base::Bind(sink_transition_callback_.callback(), true));
    }
  }

  return dest->frames();
}

void SilentSinkSuspender::OnRenderError() {
  callback_->OnRenderError();
}

void SilentSinkSuspender::TransitionSinks(bool use_fake_sink) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  // Ignore duplicate requests which can occur if the transition takes too long
  // and multiple Render() events occur.
  if (use_fake_sink == is_using_fake_sink_)
    return;

  if (use_fake_sink) {
    sink_->Pause();

    // |sink_| may still be executing Render() at this point or even sometime
    // after this point, so we must acquire the lock to make sure we don't have
    // concurrent Render() execution. Once |is_using_fake_sink_| is set to true,
    // calls from |sink_| will be dropped.
    {
      base::AutoLock al(transition_lock_);
      is_transition_pending_ = false;
      is_using_fake_sink_ = true;
    }
    fake_sink_.Start(
        base::Bind(base::IgnoreResult(&SilentSinkSuspender::Render),
                   base::Unretained(this), base::TimeDelta(),
                   base::TimeTicks::Now(), 0, nullptr));
  } else {
    fake_sink_.Stop();

    // Despite the fake sink having a synchronous Stop(), if this transition
    // occurs too soon after pausing the real sink, we may have pending Render()
    // calls from before the transition to the fake sink. As such, we need to
    // hold the lock here to avoid any races.
    {
      base::AutoLock al(transition_lock_);
      is_transition_pending_ = false;
      is_using_fake_sink_ = false;
    }
    sink_->Play();
  }
}

}  // namespace content
