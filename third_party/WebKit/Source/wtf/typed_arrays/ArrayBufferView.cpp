/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#include "wtf/typed_arrays/ArrayBufferView.h"

#include "wtf/typed_arrays/ArrayBuffer.h"

namespace WTF {

ArrayBufferView::ArrayBufferView(PassRefPtr<ArrayBuffer> buffer,
                                 unsigned byteOffset)
    : m_byteOffset(byteOffset),
      m_isNeuterable(true),
      m_buffer(buffer),
      m_prevView(nullptr),
      m_nextView(nullptr) {
  m_baseAddress = m_buffer
                      ? (static_cast<char*>(m_buffer->data()) + m_byteOffset)
                      : nullptr;
  if (m_buffer)
    m_buffer->addView(this);
}

ArrayBufferView::~ArrayBufferView() {
  if (m_buffer)
    m_buffer->removeView(this);
}

void ArrayBufferView::neuter() {
  m_buffer = nullptr;
  m_byteOffset = 0;
}

const char* ArrayBufferView::typeName() {
  switch (type()) {
    case TypeInt8:
      return "Int8";
      break;
    case TypeUint8:
      return "UInt8";
      break;
    case TypeUint8Clamped:
      return "UInt8Clamped";
      break;
    case TypeInt16:
      return "Int16";
      break;
    case TypeUint16:
      return "UInt16";
      break;
    case TypeInt32:
      return "Int32";
      break;
    case TypeUint32:
      return "Uint32";
      break;
    case TypeFloat32:
      return "Float32";
      break;
    case TypeFloat64:
      return "Float64";
      break;
    case TypeDataView:
      return "DataView";
      break;
  }
  NOTREACHED();
  return "Unknown";
}

}  // namespace WTF
