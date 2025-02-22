/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/peerconnection/RTCSessionDescriptionRequestImpl.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/peerconnection/RTCPeerConnection.h"
#include "modules/peerconnection/RTCPeerConnectionErrorCallback.h"
#include "modules/peerconnection/RTCSessionDescription.h"
#include "modules/peerconnection/RTCSessionDescriptionCallback.h"
#include "public/platform/WebRTCSessionDescription.h"
#include "wtf/RefPtr.h"

namespace blink {

RTCSessionDescriptionRequestImpl* RTCSessionDescriptionRequestImpl::create(
    ExecutionContext* context,
    RTCPeerConnection* requester,
    RTCSessionDescriptionCallback* successCallback,
    RTCPeerConnectionErrorCallback* errorCallback) {
  return new RTCSessionDescriptionRequestImpl(context, requester,
                                              successCallback, errorCallback);
}

RTCSessionDescriptionRequestImpl::RTCSessionDescriptionRequestImpl(
    ExecutionContext* context,
    RTCPeerConnection* requester,
    RTCSessionDescriptionCallback* successCallback,
    RTCPeerConnectionErrorCallback* errorCallback)
    : ContextLifecycleObserver(context),
      m_successCallback(successCallback),
      m_errorCallback(errorCallback),
      m_requester(requester) {
  DCHECK(m_requester);
}

RTCSessionDescriptionRequestImpl::~RTCSessionDescriptionRequestImpl() {}

void RTCSessionDescriptionRequestImpl::requestSucceeded(
    const WebRTCSessionDescription& webSessionDescription) {
  bool shouldFireCallback =
      m_requester ? m_requester->shouldFireDefaultCallbacks() : false;
  if (shouldFireCallback && m_successCallback)
    m_successCallback->handleEvent(
        RTCSessionDescription::create(webSessionDescription));
  clear();
}

void RTCSessionDescriptionRequestImpl::requestFailed(const String& error) {
  bool shouldFireCallback =
      m_requester ? m_requester->shouldFireDefaultCallbacks() : false;
  if (shouldFireCallback && m_errorCallback)
    m_errorCallback->handleEvent(DOMException::create(OperationError, error));

  clear();
}

void RTCSessionDescriptionRequestImpl::contextDestroyed() {
  clear();
}

void RTCSessionDescriptionRequestImpl::clear() {
  m_successCallback.clear();
  m_errorCallback.clear();
  m_requester.clear();
}

DEFINE_TRACE(RTCSessionDescriptionRequestImpl) {
  visitor->trace(m_successCallback);
  visitor->trace(m_errorCallback);
  visitor->trace(m_requester);
  RTCSessionDescriptionRequest::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
