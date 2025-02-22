/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "core/dom/SuspendableObject.h"

#include "core/dom/ExecutionContext.h"
#include "platform/InstanceCounters.h"

namespace blink {

SuspendableObject::SuspendableObject(ExecutionContext* executionContext)
    : ContextLifecycleObserver(executionContext, SuspendableObjectType)
#if DCHECK_IS_ON()
      ,
      m_suspendIfNeededCalled(false)
#endif
{
  DCHECK(!executionContext || executionContext->isContextThread());
  InstanceCounters::incrementCounter(
      InstanceCounters::SuspendableObjectCounter);
}

SuspendableObject::~SuspendableObject() {
  InstanceCounters::decrementCounter(
      InstanceCounters::SuspendableObjectCounter);

#if DCHECK_IS_ON()
  DCHECK(m_suspendIfNeededCalled);
#endif
}

void SuspendableObject::suspendIfNeeded() {
#if DCHECK_IS_ON()
  DCHECK(!m_suspendIfNeededCalled);
  m_suspendIfNeededCalled = true;
#endif
  if (ExecutionContext* context = getExecutionContext())
    context->suspendSuspendableObjectIfNeeded(this);
}

void SuspendableObject::suspend() {}

void SuspendableObject::resume() {}

void SuspendableObject::didMoveToNewExecutionContext(
    ExecutionContext* context) {
  setContext(context);

  if (context->isContextDestroyed()) {
    contextDestroyed();
    return;
  }

  if (context->isContextSuspended()) {
    suspend();
    return;
  }

  resume();
}

}  // namespace blink
