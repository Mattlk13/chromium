/*
 * Copyright (C) 2007, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Justin Haygood (jhaygood@reaktix.com)
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

#ifndef Threading_h
#define Threading_h

#include "wtf/Atomics.h"
#include "wtf/TypeTraits.h"
#include "wtf/WTFExport.h"
#include <stdint.h>

// For portability, we do not make use of C++11 thread-safe statics, as
// supported by some toolchains. Make use of double-checked locking to reduce
// overhead.  Note that this uses system-wide default lock, and cannot be used
// before WTF::initializeThreading() is called.
#define DEFINE_THREAD_SAFE_STATIC_LOCAL(T, name, initializer)             \
  static_assert(!WTF::IsGarbageCollectedType<T>::value,                   \
                "Garbage collected types should not be a static local!"); \
  /* Init to nullptr is thread-safe on all implementations. */            \
  static void* name##Pointer = nullptr;                                   \
  if (!WTF::acquireLoad(&name##Pointer)) {                                \
    WTF::lockAtomicallyInitializedStaticMutex();                          \
    if (!WTF::acquireLoad(&name##Pointer)) {                              \
      std::remove_const<T>::type* initializerResult = initializer;        \
      WTF::releaseStore(&name##Pointer, initializerResult);               \
    }                                                                     \
    WTF::unlockAtomicallyInitializedStaticMutex();                        \
  }                                                                       \
  T& name = *static_cast<T*>(name##Pointer)

namespace WTF {

#if OS(WIN)
typedef uint32_t ThreadIdentifier;
#else
typedef intptr_t ThreadIdentifier;
#endif

WTF_EXPORT ThreadIdentifier currentThread();

WTF_EXPORT void lockAtomicallyInitializedStaticMutex();
WTF_EXPORT void unlockAtomicallyInitializedStaticMutex();

#if DCHECK_IS_ON()
WTF_EXPORT bool isAtomicallyInitializedStaticMutexLockHeld();
WTF_EXPORT bool isBeforeThreadCreated();
WTF_EXPORT void willCreateThread();
#endif

}  // namespace WTF

using WTF::ThreadIdentifier;
using WTF::currentThread;

#endif  // Threading_h
