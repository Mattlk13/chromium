// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/timing/DOMWindowPerformance.h"

#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/timing/Performance.h"

namespace blink {

DOMWindowPerformance::DOMWindowPerformance(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

DEFINE_TRACE(DOMWindowPerformance) {
  visitor->trace(m_performance);
  Supplement<LocalDOMWindow>::trace(visitor);
}

// static
const char* DOMWindowPerformance::supplementName() {
  return "DOMWindowPerformance";
}

// static
DOMWindowPerformance& DOMWindowPerformance::from(LocalDOMWindow& window) {
  DOMWindowPerformance* supplement = static_cast<DOMWindowPerformance*>(
      Supplement<LocalDOMWindow>::from(window, supplementName()));
  if (!supplement) {
    supplement = new DOMWindowPerformance(window);
    provideTo(window, supplementName(), supplement);
  }
  return *supplement;
}

// static
Performance* DOMWindowPerformance::performance(DOMWindow& window) {
  return from(toLocalDOMWindow(window)).performance();
}

Performance* DOMWindowPerformance::performance() {
  if (!m_performance)
    m_performance = Performance::create(host()->frame());
  return m_performance.get();
}

}  // namespace blink
