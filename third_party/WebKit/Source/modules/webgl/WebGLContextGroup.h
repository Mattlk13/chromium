/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 */

#ifndef WebGLContextGroup_h
#define WebGLContextGroup_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/webgl/WebGLRenderingContextBase.h"
#include "wtf/HashSet.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"

namespace blink {

typedef int ExceptionCode;

class WebGLContextGroup final : public GarbageCollected<WebGLContextGroup>,
                                public TraceWrapperBase {
  WTF_MAKE_NONCOPYABLE(WebGLContextGroup);

 public:
  WebGLContextGroup();

  void addContext(WebGLRenderingContextBase*);

  // There's no point in having a removeContext method any more now that
  // the context group is GarbageCollected. The only time it would be
  // called would be during WebGLRenderingContext destruction, and at that
  // time, the context is not allowed to refer back to the context group
  // since both are on the Oilpan heap.

  gpu::gles2::GLES2Interface* getAGLInterface();

  void loseContextGroup(WebGLRenderingContextBase::LostContextMode,
                        WebGLRenderingContextBase::AutoRecoveryMethod);

  // This counter gets incremented every time context loss is
  // triggered. Because there's no longer any explicit enumeration of
  // the objects in a given context group upon context loss, each
  // object needs to keep track of the context loss count when it was
  // created, in order to validate itself.
  uint32_t numberOfContextLosses() const;

  DEFINE_INLINE_TRACE() { visitor->trace(m_contexts); }

  DECLARE_VIRTUAL_TRACE_WRAPPERS();

 private:
  friend class WebGLObject;

  uint32_t m_numberOfContextLosses;

  HeapHashSet<TraceWrapperMember<WebGLRenderingContextBase>> m_contexts;
};

}  // namespace blink

#endif  // WebGLContextGroup_h
