/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef ArrayBufferContents_h
#define ArrayBufferContents_h

#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/WTF.h"
#include "wtf/WTFExport.h"

namespace WTF {

class WTF_EXPORT ArrayBufferContents {
  WTF_MAKE_NONCOPYABLE(ArrayBufferContents);
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  using AdjustAmountOfExternalAllocatedMemoryFunction = void (*)(int64_t diff);

  enum InitializationPolicy { ZeroInitialize, DontInitialize };

  enum SharingType {
    NotShared,
    Shared,
  };

  ArrayBufferContents();
  ArrayBufferContents(unsigned numElements,
                      unsigned elementByteSize,
                      SharingType isShared,
                      InitializationPolicy);

  // Use with care. data must be allocated with allocateMemory.
  // ArrayBufferContents will take ownership of the data and free it (using
  // freeMemory) upon destruction.
  // This constructor will not call observer->StartObserving(), so it is a
  // responsibility of the caller to make sure JS knows about external memory.
  ArrayBufferContents(void* data, unsigned sizeInBytes, SharingType isShared);

  ~ArrayBufferContents();

  void neuter();

  void* data() const { return m_holder ? m_holder->data() : nullptr; }
  unsigned sizeInBytes() const {
    return m_holder ? m_holder->sizeInBytes() : 0;
  }
  bool isShared() const { return m_holder ? m_holder->isShared() : false; }

  void transfer(ArrayBufferContents& other);
  void shareWith(ArrayBufferContents& other);
  void copyTo(ArrayBufferContents& other);

  static void allocateMemory(size_t, InitializationPolicy, void*&);
  static void allocateMemoryOrNull(size_t, InitializationPolicy, void*&);
  static void freeMemory(void*, size_t);
  static void initialize(
      AdjustAmountOfExternalAllocatedMemoryFunction function) {
    DCHECK(isMainThread());
    DCHECK_EQ(s_adjustAmountOfExternalAllocatedMemoryFunction,
              defaultAdjustAmountOfExternalAllocatedMemoryFunction);
    s_adjustAmountOfExternalAllocatedMemoryFunction = function;
  }

 private:
  static void allocateMemoryWithFlags(size_t,
                                      InitializationPolicy,
                                      int,
                                      void*&);

  static void defaultAdjustAmountOfExternalAllocatedMemoryFunction(
      int64_t diff);

  class DataHolder : public ThreadSafeRefCounted<DataHolder> {
    WTF_MAKE_NONCOPYABLE(DataHolder);

   public:
    DataHolder();
    ~DataHolder();

    void allocateNew(unsigned sizeInBytes,
                     SharingType isShared,
                     InitializationPolicy);
    void adopt(void* data, unsigned sizeInBytes, SharingType isShared);
    void copyMemoryFrom(const DataHolder& source);

    const void* data() const { return m_data; }
    void* data() { return m_data; }
    unsigned sizeInBytes() const { return m_sizeInBytes; }
    bool isShared() const { return m_isShared == Shared; }

   private:
    void adjustAmountOfExternalAllocatedMemory(int64_t diff) {
      checkIfAdjustAmountOfExternalAllocatedMemoryIsConsistent();
      s_adjustAmountOfExternalAllocatedMemoryFunction(diff);
    }
    void adjustAmountOfExternalAllocatedMemory(unsigned diff) {
      adjustAmountOfExternalAllocatedMemory(static_cast<int64_t>(diff));
    }

    void checkIfAdjustAmountOfExternalAllocatedMemoryIsConsistent() {
      DCHECK(s_adjustAmountOfExternalAllocatedMemoryFunction);

#if DCHECK_IS_ON()
      // Make sure that the function actually used is always the same.
      // Shouldn't be updated during its use.
      if (!s_lastUsedAdjustAmountOfExternalAllocatedMemoryFunction) {
        s_lastUsedAdjustAmountOfExternalAllocatedMemoryFunction =
            s_adjustAmountOfExternalAllocatedMemoryFunction;
      }
      DCHECK_EQ(s_adjustAmountOfExternalAllocatedMemoryFunction,
                s_lastUsedAdjustAmountOfExternalAllocatedMemoryFunction);
#endif
    }

    void* m_data;
    unsigned m_sizeInBytes;
    SharingType m_isShared;
  };

  RefPtr<DataHolder> m_holder;
  static AdjustAmountOfExternalAllocatedMemoryFunction
      s_adjustAmountOfExternalAllocatedMemoryFunction;
#if DCHECK_IS_ON()
  static AdjustAmountOfExternalAllocatedMemoryFunction
      s_lastUsedAdjustAmountOfExternalAllocatedMemoryFunction;
#endif
};

}  // namespace WTF

#endif  // ArrayBufferContents_h
