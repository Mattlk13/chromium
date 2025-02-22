// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationRequest.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/loader/MixedContentChecker.h"
#include "modules/EventTargetModules.h"
#include "modules/presentation/ExistingPresentationConnectionCallbacks.h"
#include "modules/presentation/PresentationAvailability.h"
#include "modules/presentation/PresentationAvailabilityCallbacks.h"
#include "modules/presentation/PresentationConnection.h"
#include "modules/presentation/PresentationConnectionCallbacks.h"
#include "modules/presentation/PresentationController.h"
#include "modules/presentation/PresentationError.h"
#include "platform/UserGestureIndicator.h"

namespace blink {

namespace {

// TODO(mlamouri): refactor in one common place.
PresentationController* presentationController(
    ExecutionContext* executionContext) {
  DCHECK(executionContext);

  Document* document = toDocument(executionContext);
  if (!document->frame())
    return nullptr;
  return PresentationController::from(*document->frame());
}

WebPresentationClient* presentationClient(ExecutionContext* executionContext) {
  PresentationController* controller = presentationController(executionContext);
  return controller ? controller->client() : nullptr;
}

Settings* settings(ExecutionContext* executionContext) {
  DCHECK(executionContext);

  Document* document = toDocument(executionContext);
  return document->settings();
}

ScriptPromise rejectWithMixedContentException(ScriptState* scriptState,
                                              const String& url) {
  return ScriptPromise::rejectWithDOMException(
      scriptState,
      DOMException::create(SecurityError,
                           "Presentation of an insecure document [" + url +
                               "] is prohibited from a secure context."));
}

ScriptPromise rejectWithSandBoxException(ScriptState* scriptState) {
  return ScriptPromise::rejectWithDOMException(
      scriptState, DOMException::create(SecurityError,
                                        "The document is sandboxed and lacks "
                                        "the 'allow-presentation' flag."));
}

}  // anonymous namespace

// static
PresentationRequest* PresentationRequest::create(
    ExecutionContext* executionContext,
    const String& url,
    ExceptionState& exceptionState) {
  KURL parsedUrl = KURL(executionContext->url(), url);
  if (!parsedUrl.isValid() || parsedUrl.protocolIsAbout()) {
    exceptionState.throwTypeError("'" + url +
                                  "' can't be resolved to a valid URL.");
    return nullptr;
  }

  return new PresentationRequest(executionContext, parsedUrl);
}

const AtomicString& PresentationRequest::interfaceName() const {
  return EventTargetNames::PresentationRequest;
}

ExecutionContext* PresentationRequest::getExecutionContext() const {
  return ContextLifecycleObserver::getExecutionContext();
}

void PresentationRequest::addedEventListener(
    const AtomicString& eventType,
    RegisteredEventListener& registeredListener) {
  EventTargetWithInlineData::addedEventListener(eventType, registeredListener);
  if (eventType == EventTypeNames::connectionavailable)
    UseCounter::count(
        getExecutionContext(),
        UseCounter::PresentationRequestConnectionAvailableEventListener);
}

bool PresentationRequest::hasPendingActivity() const {
  // Prevents garbage collecting of this object when not hold by another
  // object but still has listeners registered.
  return getExecutionContext() && hasEventListeners();
}

ScriptPromise PresentationRequest::start(ScriptState* scriptState) {
  Settings* contextSettings = settings(getExecutionContext());
  bool isUserGestureRequired =
      !contextSettings || contextSettings->getPresentationRequiresUserGesture();

  if (isUserGestureRequired && !UserGestureIndicator::utilizeUserGesture())
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(
            InvalidAccessError,
            "PresentationRequest::start() requires user gesture."));

  if (MixedContentChecker::isMixedContent(
          getExecutionContext()->getSecurityOrigin(), m_url)) {
    return rejectWithMixedContentException(scriptState, m_url.getString());
  }

  if (toDocument(getExecutionContext())->isSandboxed(SandboxPresentation))
    return rejectWithSandBoxException(scriptState);

  WebPresentationClient* client = presentationClient(getExecutionContext());
  if (!client)
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(
            InvalidStateError,
            "The PresentationRequest is no longer associated to a frame."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  // TODO(crbug.com/627655): Accept multiple URLs per PresentationRequest.
  WebVector<WebURL> presentationUrls(static_cast<size_t>(1U));
  presentationUrls[0] = m_url;
  client->startSession(
      presentationUrls,
      WTF::makeUnique<PresentationConnectionCallbacks>(resolver, this));
  return resolver->promise();
}

ScriptPromise PresentationRequest::reconnect(ScriptState* scriptState,
                                             const String& id) {
  if (MixedContentChecker::isMixedContent(
          getExecutionContext()->getSecurityOrigin(), m_url)) {
    return rejectWithMixedContentException(scriptState, m_url.getString());
  }

  if (toDocument(getExecutionContext())->isSandboxed(SandboxPresentation))
    return rejectWithSandBoxException(scriptState);

  WebPresentationClient* client = presentationClient(getExecutionContext());
  if (!client)
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(
            InvalidStateError,
            "The PresentationRequest is no longer associated to a frame."));

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  // TODO(crbug.com/627655): Accept multiple URLs per PresentationRequest.
  WebVector<WebURL> presentationUrls(static_cast<size_t>(1U));
  presentationUrls[0] = m_url;

  PresentationController* controller =
      presentationController(getExecutionContext());
  DCHECK(controller);

  PresentationConnection* existingConnection =
      controller->findExistingConnection(presentationUrls, id);
  if (existingConnection) {
    client->joinSession(
        presentationUrls, id,
        WTF::makeUnique<ExistingPresentationConnectionCallbacks>(
            resolver, existingConnection));
  } else {
    client->joinSession(
        presentationUrls, id,
        WTF::makeUnique<PresentationConnectionCallbacks>(resolver, this));
  }
  return resolver->promise();
}

ScriptPromise PresentationRequest::getAvailability(ScriptState* scriptState) {
  if (MixedContentChecker::isMixedContent(
          getExecutionContext()->getSecurityOrigin(), m_url)) {
    return rejectWithMixedContentException(scriptState, m_url.getString());
  }

  if (toDocument(getExecutionContext())->isSandboxed(SandboxPresentation))
    return rejectWithSandBoxException(scriptState);

  WebPresentationClient* client = presentationClient(getExecutionContext());
  if (!client)
    return ScriptPromise::rejectWithDOMException(
        scriptState,
        DOMException::create(
            InvalidStateError,
            "The PresentationRequest is no longer associated to a frame."));

  if (!m_availabilityProperty) {
    m_availabilityProperty = new PresentationAvailabilityProperty(
        scriptState->getExecutionContext(), this,
        PresentationAvailabilityProperty::Ready);

    client->getAvailability(m_url,
                            WTF::makeUnique<PresentationAvailabilityCallbacks>(
                                m_availabilityProperty, m_url));
  }
  return m_availabilityProperty->promise(scriptState->world());
}

const KURL& PresentationRequest::url() const {
  return m_url;
}

DEFINE_TRACE(PresentationRequest) {
  visitor->trace(m_availabilityProperty);
  EventTargetWithInlineData::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

PresentationRequest::PresentationRequest(ExecutionContext* executionContext,
                                         const KURL& url)
    : ContextLifecycleObserver(executionContext), m_url(url) {}

}  // namespace blink
