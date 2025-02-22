// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/storage/DOMWindowStorage.h"

#include "core/dom/Document.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "modules/storage/Storage.h"
#include "modules/storage/StorageNamespace.h"
#include "modules/storage/StorageNamespaceController.h"
#include "wtf/PassRefPtr.h"

namespace blink {

DOMWindowStorage::DOMWindowStorage(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

DEFINE_TRACE(DOMWindowStorage) {
  visitor->trace(m_sessionStorage);
  visitor->trace(m_localStorage);
  Supplement<LocalDOMWindow>::trace(visitor);
}

// static
const char* DOMWindowStorage::supplementName() {
  return "DOMWindowStorage";
}

// static
DOMWindowStorage& DOMWindowStorage::from(LocalDOMWindow& window) {
  DOMWindowStorage* supplement = static_cast<DOMWindowStorage*>(
      Supplement<LocalDOMWindow>::from(window, supplementName()));
  if (!supplement) {
    supplement = new DOMWindowStorage(window);
    provideTo(window, supplementName(), supplement);
  }
  return *supplement;
}

// static
Storage* DOMWindowStorage::sessionStorage(DOMWindow& window,
                                          ExceptionState& exceptionState) {
  return from(toLocalDOMWindow(window)).sessionStorage(exceptionState);
}

// static
Storage* DOMWindowStorage::localStorage(DOMWindow& window,
                                        ExceptionState& exceptionState) {
  return from(toLocalDOMWindow(window)).localStorage(exceptionState);
}

Storage* DOMWindowStorage::sessionStorage(
    ExceptionState& exceptionState) const {
  if (!host()->frame())
    return nullptr;

  Document* document = host()->frame()->document();
  DCHECK(document);
  String accessDeniedMessage = "Access is denied for this document.";
  if (!document->getSecurityOrigin()->canAccessLocalStorage()) {
    if (document->isSandboxed(SandboxOrigin))
      exceptionState.throwSecurityError(
          "The document is sandboxed and lacks the 'allow-same-origin' flag.");
    else if (document->url().protocolIs("data"))
      exceptionState.throwSecurityError(
          "Storage is disabled inside 'data:' URLs.");
    else
      exceptionState.throwSecurityError(accessDeniedMessage);
    return nullptr;
  }

  if (m_sessionStorage) {
    if (!m_sessionStorage->area()->canAccessStorage(document->frame())) {
      exceptionState.throwSecurityError(accessDeniedMessage);
      return nullptr;
    }
    return m_sessionStorage;
  }

  Page* page = document->page();
  if (!page)
    return nullptr;

  StorageArea* storageArea =
      StorageNamespaceController::from(page)->sessionStorage()->storageArea(
          document->getSecurityOrigin());
  if (!storageArea->canAccessStorage(document->frame())) {
    exceptionState.throwSecurityError(accessDeniedMessage);
    return nullptr;
  }

  m_sessionStorage = Storage::create(document->frame(), storageArea);
  return m_sessionStorage;
}

Storage* DOMWindowStorage::localStorage(ExceptionState& exceptionState) const {
  if (!host()->frame())
    return nullptr;

  Document* document = host()->frame()->document();
  DCHECK(document);
  String accessDeniedMessage = "Access is denied for this document.";
  if (!document->getSecurityOrigin()->canAccessLocalStorage()) {
    if (document->isSandboxed(SandboxOrigin))
      exceptionState.throwSecurityError(
          "The document is sandboxed and lacks the 'allow-same-origin' flag.");
    else if (document->url().protocolIs("data"))
      exceptionState.throwSecurityError(
          "Storage is disabled inside 'data:' URLs.");
    else
      exceptionState.throwSecurityError(accessDeniedMessage);
    return nullptr;
  }
  if (m_localStorage) {
    if (!m_localStorage->area()->canAccessStorage(document->frame())) {
      exceptionState.throwSecurityError(accessDeniedMessage);
      return nullptr;
    }
    return m_localStorage;
  }
  // FIXME: Seems this check should be much higher?
  FrameHost* host = document->frameHost();
  if (!host || !host->settings().getLocalStorageEnabled())
    return nullptr;
  StorageArea* storageArea =
      StorageNamespace::localStorageArea(document->getSecurityOrigin());
  if (!storageArea->canAccessStorage(document->frame())) {
    exceptionState.throwSecurityError(accessDeniedMessage);
    return nullptr;
  }
  m_localStorage = Storage::create(document->frame(), storageArea);
  return m_localStorage;
}

}  // namespace blink
