// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TaskRunnerHelper_h
#define TaskRunnerHelper_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/HashTraits.h"

namespace blink {

class Document;
class ExecutionContext;
class LocalFrame;
class ScriptState;
class WebTaskRunner;

enum class TaskType : unsigned {
  // Speced tasks and related internal tasks should be posted to one of
  // the following task runners. These task runners may be throttled.
  DOMManipulation,
  UserInteraction,
  Networking,
  HistoryTraversal,
  Embed,
  MediaElementEvent,
  CanvasBlobSerialization,
  Microtask,
  Timer,
  RemoteEvent,
  WebSocket,
  PostedMessage,
  UnshippedPortMessage,
  FileReading,
  DatabaseAccess,
  Presentation,
  Sensor,

  // Other internal tasks that cannot fit any of the above task runners
  // can be posted here, but the usage is not encouraged. The task runner
  // may be throttled.
  //
  // UnspecedLoading type should be used for all tasks associated with
  // loading page content, UnspecedTimer should be used for all other purposes.
  UnspecedTimer,
  UnspecedLoading,

  // Tasks that must not be throttled should be posted here, but the usage
  // should be very limited.
  Unthrottled,

  // Tasks that any other TaskType is not assigned to. This should be
  // transitional and should be removed.
  Unspecified,
};

// HashTraits for TaskType.
struct TaskTypeTraits : WTF::GenericHashTraits<TaskType> {
  static const bool emptyValueIsZero = false;
  static TaskType emptyValue() { return static_cast<TaskType>(-1); }
  static void constructDeletedValue(TaskType& slot, bool) {
    slot = static_cast<TaskType>(-2);
  }
  static bool isDeletedValue(TaskType value) {
    return value == static_cast<TaskType>(-2);
  }
};

class CORE_EXPORT TaskRunnerHelper final {
  STATIC_ONLY(TaskRunnerHelper);

 public:
  static WebTaskRunner* get(TaskType, LocalFrame*);
  static WebTaskRunner* get(TaskType, Document*);
  static WebTaskRunner* get(TaskType, ExecutionContext*);
  static WebTaskRunner* get(TaskType, ScriptState*);
};

}  // namespace blink

#endif
