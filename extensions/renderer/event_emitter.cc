// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/event_emitter.h"

#include <algorithm>

#include "gin/object_template_builder.h"
#include "gin/per_context_data.h"

namespace extensions {

gin::WrapperInfo EventEmitter::kWrapperInfo = {gin::kEmbedderNativeGin};

EventEmitter::EventEmitter(const binding::RunJSFunction& run_js)
    : run_js_(run_js) {}

EventEmitter::~EventEmitter() {}

gin::ObjectTemplateBuilder EventEmitter::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return Wrappable<EventEmitter>::GetObjectTemplateBuilder(isolate)
      .SetMethod("addListener", &EventEmitter::AddListener)
      .SetMethod("removeListener", &EventEmitter::RemoveListener)
      .SetMethod("hasListener", &EventEmitter::HasListener)
      .SetMethod("hasListeners", &EventEmitter::HasListeners)
      // The following methods aren't part of the public API, but are used
      // by our custom bindings and exposed on the public event object. :(
      // TODO(devlin): Once we convert all custom bindings that use these,
      // they can be removed.
      .SetMethod("dispatch", &EventEmitter::Dispatch);
}

void EventEmitter::Fire(v8::Local<v8::Context> context,
                        std::vector<v8::Local<v8::Value>>* args) {
  // We create a local copy of listeners_ since the array can be modified during
  // handling.
  std::vector<v8::Local<v8::Function>> listeners;
  listeners.reserve(listeners_.size());
  for (const auto& listener : listeners_)
    listeners.push_back(listener.Get(context->GetIsolate()));

  for (const auto& listener : listeners)
    run_js_.Run(listener, context, args->size(), args->data());
}

void EventEmitter::AddListener(gin::Arguments* arguments) {
  v8::Local<v8::Function> listener;
  if (!arguments->GetNext(&listener))
    return;

  v8::Local<v8::Object> holder;
  CHECK(arguments->GetHolder(&holder));
  CHECK(!holder.IsEmpty());
  if (!gin::PerContextData::From(holder->CreationContext()))
    return;

  if (!HasListener(listener)) {
    listeners_.push_back(
        v8::Global<v8::Function>(arguments->isolate(), listener));
  }
}

void EventEmitter::RemoveListener(v8::Local<v8::Function> listener) {
  auto iter = std::find(listeners_.begin(), listeners_.end(), listener);
  if (iter != listeners_.end())
    listeners_.erase(iter);
}

bool EventEmitter::HasListener(v8::Local<v8::Function> listener) {
  return std::find(listeners_.begin(), listeners_.end(), listener) !=
         listeners_.end();
}

bool EventEmitter::HasListeners() {
  return !listeners_.empty();
}

void EventEmitter::Dispatch(gin::Arguments* arguments) {
  if (listeners_.empty())
    return;
  v8::HandleScope handle_scope(arguments->isolate());
  v8::Local<v8::Context> context =
      arguments->isolate()->GetCurrentContext();
  std::vector<v8::Local<v8::Value>> v8_args;
  if (arguments->Length() > 0) {
    // Converting to v8::Values should never fail.
    CHECK(arguments->GetRemaining(&v8_args));
  }
  Fire(context, &v8_args);
}

}  // namespace extensions
