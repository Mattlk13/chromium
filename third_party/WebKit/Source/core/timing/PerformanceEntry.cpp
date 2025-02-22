/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#include "core/timing/PerformanceEntry.h"

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8ObjectBuilder.h"

namespace blink {

PerformanceEntry::PerformanceEntry(const String& name,
                                   const String& entryType,
                                   double startTime,
                                   double finishTime)
    : m_name(name),
      m_entryType(entryType),
      m_startTime(startTime),
      m_duration(finishTime - startTime),
      m_entryTypeEnum(toEntryTypeEnum(entryType)) {}

PerformanceEntry::~PerformanceEntry() {}

String PerformanceEntry::name() const {
  return m_name;
}

String PerformanceEntry::entryType() const {
  return m_entryType;
}

double PerformanceEntry::startTime() const {
  return m_startTime;
}

double PerformanceEntry::duration() const {
  return m_duration;
}

PerformanceEntry::EntryType PerformanceEntry::toEntryTypeEnum(
    const String& entryType) {
  if (entryType == "composite")
    return Composite;
  if (entryType == "longtask")
    return LongTask;
  if (entryType == "mark")
    return Mark;
  if (entryType == "measure")
    return Measure;
  if (entryType == "render")
    return Render;
  if (entryType == "resource")
    return Resource;
  if (entryType == "navigation")
    return Navigation;
  if (entryType == "taskattribution")
    return TaskAttribution;
  return Invalid;
}

ScriptValue PerformanceEntry::toJSONForBinding(ScriptState* scriptState) const {
  V8ObjectBuilder result(scriptState);
  buildJSONValue(result);
  return result.scriptValue();
}

void PerformanceEntry::buildJSONValue(V8ObjectBuilder& builder) const {
  builder.addString("name", name());
  builder.addString("entryType", entryType());
  builder.addNumber("startTime", startTime());
  builder.addNumber("duration", duration());
}

}  // namespace blink
