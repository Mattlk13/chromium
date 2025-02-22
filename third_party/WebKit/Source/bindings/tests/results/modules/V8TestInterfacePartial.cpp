// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated by code_generator_v8.py.
// DO NOT MODIFY!

// This file has been generated from the Jinja2 template in
// third_party/WebKit/Source/bindings/templates/partial_interface.cpp.tmpl

// clang-format off
#include "V8TestInterfacePartial.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/GeneratedCodeHelper.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8DOMConfiguration.h"
#include "bindings/core/v8/V8Document.h"
#include "bindings/core/v8/V8Node.h"
#include "bindings/core/v8/V8ObjectConstructor.h"
#include "bindings/core/v8/V8TestInterface.h"
#include "bindings/tests/idls/modules/TestInterfacePartial3Implementation.h"
#include "bindings/tests/idls/modules/TestInterfacePartial4.h"
#include "core/dom/Document.h"
#include "core/origin_trials/OriginTrials.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/GetPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

namespace TestInterfaceImplementationPartialV8Internal {

static void partial4LongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();

  TestInterfaceImplementation* impl = V8TestInterface::toImpl(holder);

  v8SetReturnValueInt(info, TestInterfacePartial4::partial4LongAttribute(*impl));
}

void partial4LongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementationPartialV8Internal::partial4LongAttributeAttributeGetter(info);
}

static void partial4LongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Object> holder = info.Holder();
  TestInterfaceImplementation* impl = V8TestInterface::toImpl(holder);

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::SetterContext, "TestInterface", "partial4LongAttribute");

  // Prepare the value to be set.
  int cppValue = toInt32(info.GetIsolate(), v8Value, NormalConversion, exceptionState);
  if (exceptionState.hadException())
    return;

  TestInterfacePartial4::setPartial4LongAttribute(*impl, cppValue);
}

void partial4LongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Value> v8Value = info[0];

  TestInterfaceImplementationPartialV8Internal::partial4LongAttributeAttributeSetter(v8Value, info);
}

static void partial4StaticLongAttributeAttributeGetter(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8SetReturnValueInt(info, TestInterfacePartial4::partial4StaticLongAttribute());
}

void partial4StaticLongAttributeAttributeGetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementationPartialV8Internal::partial4StaticLongAttributeAttributeGetter(info);
}

static void partial4StaticLongAttributeAttributeSetter(v8::Local<v8::Value> v8Value, const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::SetterContext, "TestInterface", "partial4StaticLongAttribute");

  // Prepare the value to be set.
  int cppValue = toInt32(info.GetIsolate(), v8Value, NormalConversion, exceptionState);
  if (exceptionState.hadException())
    return;

  TestInterfacePartial4::setPartial4StaticLongAttribute(cppValue);
}

void partial4StaticLongAttributeAttributeSetterCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Local<v8::Value> v8Value = info[0];

  TestInterfaceImplementationPartialV8Internal::partial4StaticLongAttributeAttributeSetter(v8Value, info);
}

static void voidMethodPartialOverload3Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::toImpl(info.Holder());

  V8StringResource<> value;
  value = info[0];
  if (!value.prepare())
    return;

  TestInterfacePartial3Implementation::voidMethodPartialOverload(*impl, value);
}

static void voidMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;
  switch (std::min(1, info.Length())) {
    case 0:
      break;
    case 1:
      if (true) {
        voidMethodPartialOverload3Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::ExecutionContext, "TestInterface", "voidMethodPartialOverload");

  if (isArityError) {
  }
  exceptionState.throwTypeError("No function was found that matched the signature provided.");
}

static void staticVoidMethodPartialOverload2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8StringResource<> value;
  value = info[0];
  if (!value.prepare())
    return;

  TestInterfacePartial3Implementation::staticVoidMethodPartialOverload(value);
}

static void staticVoidMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;
  switch (std::min(1, info.Length())) {
    case 0:
      break;
    case 1:
      if (true) {
        staticVoidMethodPartialOverload2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::ExecutionContext, "TestInterface", "staticVoidMethodPartialOverload");

  if (isArityError) {
  }
  exceptionState.throwTypeError("No function was found that matched the signature provided.");
}

static void promiseMethodPartialOverload3Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::ExecutionContext, "TestInterface", "promiseMethodPartialOverload");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  // V8DOMConfiguration::DoNotCheckHolder
  // Make sure that info.Holder() really points to an instance of the type.
  if (!V8TestInterface::hasInstance(info.Holder(), info.GetIsolate())) {
    exceptionState.throwTypeError("Illegal invocation");
    return;
  }
  TestInterfaceImplementation* impl = V8TestInterface::toImpl(info.Holder());

  Document* document;
  document = V8Document::toImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!document) {
    exceptionState.throwTypeError("parameter 1 is not of type 'Document'.");

    return;
  }

  v8SetReturnValue(info, TestInterfacePartial3Implementation::promiseMethodPartialOverload(*impl, document).v8Value());
}

static void promiseMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;
  switch (std::min(1, info.Length())) {
    case 0:
      break;
    case 1:
      if (V8Document::hasInstance(info[0], info.GetIsolate())) {
        promiseMethodPartialOverload3Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::ExecutionContext, "TestInterface", "promiseMethodPartialOverload");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  if (isArityError) {
  }
  exceptionState.throwTypeError("No function was found that matched the signature provided.");
}

static void staticPromiseMethodPartialOverload2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::ExecutionContext, "TestInterface", "staticPromiseMethodPartialOverload");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  V8StringResource<> value;
  value = info[0];
  if (!value.prepare(exceptionState))
    return;

  v8SetReturnValue(info, TestInterfacePartial3Implementation::staticPromiseMethodPartialOverload(value).v8Value());
}

static void staticPromiseMethodPartialOverloadMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;
  switch (std::min(1, info.Length())) {
    case 0:
      break;
    case 1:
      if (true) {
        staticPromiseMethodPartialOverload2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::ExecutionContext, "TestInterface", "staticPromiseMethodPartialOverload");
  ExceptionToRejectPromiseScope rejectPromiseScope(info, exceptionState);

  if (isArityError) {
  }
  exceptionState.throwTypeError("No function was found that matched the signature provided.");
}

static void partial2VoidMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::toImpl(info.Holder());

  V8StringResource<> value;
  value = info[0];
  if (!value.prepare())
    return;

  TestInterfacePartial3Implementation::partial2VoidMethod(*impl, value);
}

static void partial2VoidMethod3Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::toImpl(info.Holder());

  Node* node;
  node = V8Node::toImplWithTypeCheck(info.GetIsolate(), info[0]);
  if (!node) {
    V8ThrowException::throwTypeError(info.GetIsolate(), ExceptionMessages::failedToExecute("partial2VoidMethod", "TestInterface", "parameter 1 is not of type 'Node'."));

    return;
  }

  TestInterfacePartial3Implementation::partial2VoidMethod(*impl, node);
}

static void partial2VoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;
  switch (std::min(1, info.Length())) {
    case 0:
      break;
    case 1:
      if (V8Node::hasInstance(info[0], info.GetIsolate())) {
        partial2VoidMethod3Method(info);
        return;
      }
      if (true) {
        partial2VoidMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::ExecutionContext, "TestInterface", "partial2VoidMethod");

  if (isArityError) {
  }
  exceptionState.throwTypeError("No function was found that matched the signature provided.");
}

static void partialVoidTestEnumModulesArgMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::ExecutionContext, "TestInterface", "partialVoidTestEnumModulesArgMethod");

  TestInterfaceImplementation* impl = V8TestInterface::toImpl(info.Holder());

  if (UNLIKELY(info.Length() < 1)) {
    exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments(1, info.Length()));
    return;
  }

  V8StringResource<> arg;
  arg = info[0];
  if (!arg.prepare())
    return;
  const char* validArgValues[] = {
      "EnumModulesValue1",
      "EnumModulesValue2",
  };
  if (!isValidEnum(arg, validArgValues, WTF_ARRAY_LENGTH(validArgValues), "TestEnumModules", exceptionState)) {
    return;
  }

  TestInterfacePartial3Implementation::partialVoidTestEnumModulesArgMethod(*impl, arg);
}

void partialVoidTestEnumModulesArgMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementationPartialV8Internal::partialVoidTestEnumModulesArgMethodMethod(info);
}

static void partial2StaticVoidMethod2Method(const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8StringResource<> value;
  value = info[0];
  if (!value.prepare())
    return;

  TestInterfacePartial3Implementation::partial2StaticVoidMethod(value);
}

static void partial2StaticVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  bool isArityError = false;
  switch (std::min(1, info.Length())) {
    case 0:
      break;
    case 1:
      if (true) {
        partial2StaticVoidMethod2Method(info);
        return;
      }
      break;
    default:
      isArityError = true;
  }

  ExceptionState exceptionState(info.GetIsolate(), ExceptionState::ExecutionContext, "TestInterface", "partial2StaticVoidMethod");

  if (isArityError) {
  }
  exceptionState.throwTypeError("No function was found that matched the signature provided.");
}

static void unscopableVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::toImpl(info.Holder());

  TestInterfacePartial3Implementation::unscopableVoidMethod(*impl);
}

void unscopableVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementationPartialV8Internal::unscopableVoidMethodMethod(info);
}

static void partial4VoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementation* impl = V8TestInterface::toImpl(info.Holder());

  TestInterfacePartial4::partial4VoidMethod(*impl);
}

void partial4VoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementationPartialV8Internal::partial4VoidMethodMethod(info);
}

static void partial4StaticVoidMethodMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfacePartial4::partial4StaticVoidMethod();
}

void partial4StaticVoidMethodMethodCallback(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TestInterfaceImplementationPartialV8Internal::partial4StaticVoidMethodMethod(info);
}

} // namespace TestInterfaceImplementationPartialV8Internal

const V8DOMConfiguration::MethodConfiguration V8TestInterfaceMethods[] = {
    {"partialVoidTestEnumModulesArgMethod", TestInterfaceImplementationPartialV8Internal::partialVoidTestEnumModulesArgMethodMethodCallback, 0, 1, v8::None, V8DOMConfiguration::OnPrototype, V8DOMConfiguration::CheckHolder},
    {"unscopableVoidMethod", TestInterfaceImplementationPartialV8Internal::unscopableVoidMethodMethodCallback, 0, 0, v8::None, V8DOMConfiguration::OnPrototype, V8DOMConfiguration::CheckHolder},
};

void V8TestInterfacePartial::installV8TestInterfaceTemplate(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::FunctionTemplate> interfaceTemplate) {
  // Initialize the interface object's template.
  V8TestInterface::installV8TestInterfaceTemplate(isolate, world, interfaceTemplate);
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  v8::Local<v8::ObjectTemplate> instanceTemplate = interfaceTemplate->InstanceTemplate();
  ALLOW_UNUSED_LOCAL(instanceTemplate);
  v8::Local<v8::ObjectTemplate> prototypeTemplate = interfaceTemplate->PrototypeTemplate();
  ALLOW_UNUSED_LOCAL(prototypeTemplate);

  // Register DOM constants, attributes and operations.
  if (RuntimeEnabledFeatures::featureNameEnabled()) {
    const V8DOMConfiguration::ConstantConfiguration V8TestInterfaceConstants[] = {
        {"PARTIAL3_UNSIGNED_SHORT", 0, 0, V8DOMConfiguration::ConstantTypeUnsignedShort},
    };
    V8DOMConfiguration::installConstants(isolate, interfaceTemplate, prototypeTemplate, V8TestInterfaceConstants, WTF_ARRAY_LENGTH(V8TestInterfaceConstants));
    V8DOMConfiguration::installMethods(isolate, world, instanceTemplate, prototypeTemplate, interfaceTemplate, signature, V8TestInterfaceMethods, WTF_ARRAY_LENGTH(V8TestInterfaceMethods));
  }
}

void V8TestInterfacePartial::installOriginTrialPartialFeature(v8::Isolate* isolate, const DOMWrapperWorld& world, v8::Local<v8::Object> instance, v8::Local<v8::Object> prototype, v8::Local<v8::Function> interface) {
  v8::Local<v8::FunctionTemplate> interfaceTemplate = V8TestInterface::wrapperTypeInfo.domTemplate(isolate, world);
  v8::Local<v8::Signature> signature = v8::Signature::New(isolate, interfaceTemplate);
  ALLOW_UNUSED_LOCAL(signature);
  const V8DOMConfiguration::AccessorConfiguration accessorpartial4LongAttributeConfiguration = {"partial4LongAttribute", TestInterfaceImplementationPartialV8Internal::partial4LongAttributeAttributeGetterCallback, TestInterfaceImplementationPartialV8Internal::partial4LongAttributeAttributeSetterCallback, 0, 0, nullptr, 0, v8::DEFAULT, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::OnPrototype, V8DOMConfiguration::CheckHolder};
  V8DOMConfiguration::installAccessor(isolate, world, instance, prototype, interface, signature, accessorpartial4LongAttributeConfiguration);
  const V8DOMConfiguration::AccessorConfiguration accessorpartial4StaticLongAttributeConfiguration = {"partial4StaticLongAttribute", TestInterfaceImplementationPartialV8Internal::partial4StaticLongAttributeAttributeGetterCallback, TestInterfaceImplementationPartialV8Internal::partial4StaticLongAttributeAttributeSetterCallback, 0, 0, nullptr, 0, v8::DEFAULT, static_cast<v8::PropertyAttribute>(v8::None), V8DOMConfiguration::OnInterface, V8DOMConfiguration::CheckHolder};
  V8DOMConfiguration::installAccessor(isolate, world, instance, prototype, interface, signature, accessorpartial4StaticLongAttributeConfiguration);
  const V8DOMConfiguration::ConstantConfiguration constantPartial4UnsignedShortConfiguration = {"PARTIAL4_UNSIGNED_SHORT", 4, 0, V8DOMConfiguration::ConstantTypeUnsignedShort};
  V8DOMConfiguration::installConstant(isolate, interface, prototype, constantPartial4UnsignedShortConfiguration);
  const V8DOMConfiguration::MethodConfiguration methodPartial4StaticvoidmethodConfiguration = {"partial4StaticVoidMethod", TestInterfaceImplementationPartialV8Internal::partial4StaticVoidMethodMethodCallback, 0, 0, v8::None, V8DOMConfiguration::OnInterface, V8DOMConfiguration::CheckHolder};
  V8DOMConfiguration::installMethod(isolate, world, instance, prototype, interface, signature, methodPartial4StaticvoidmethodConfiguration);
  const V8DOMConfiguration::MethodConfiguration methodPartial4VoidmethodConfiguration = {"partial4VoidMethod", TestInterfaceImplementationPartialV8Internal::partial4VoidMethodMethodCallback, 0, 0, v8::None, V8DOMConfiguration::OnPrototype, V8DOMConfiguration::CheckHolder};
  V8DOMConfiguration::installMethod(isolate, world, instance, prototype, interface, signature, methodPartial4VoidmethodConfiguration);
}

void V8TestInterfacePartial::installOriginTrialPartialFeature(ScriptState* scriptState, v8::Local<v8::Object> instance) {
  V8PerContextData* perContextData = V8PerContextData::from(scriptState->context());
  v8::Local<v8::Object> prototype = perContextData->prototypeForType(&V8TestInterface::wrapperTypeInfo);
  v8::Local<v8::Function> interface = perContextData->constructorForType(&V8TestInterface::wrapperTypeInfo);
  ALLOW_UNUSED_LOCAL(interface);
  installOriginTrialPartialFeature(scriptState->isolate(), scriptState->world(), instance, prototype, interface);
}

void V8TestInterfacePartial::installOriginTrialPartialFeature(ScriptState* scriptState) {
  installOriginTrialPartialFeature(scriptState, v8::Local<v8::Object>());
}

void V8TestInterfacePartial::preparePrototypeAndInterfaceObject(v8::Local<v8::Context> context, const DOMWrapperWorld& world, v8::Local<v8::Object> prototypeObject, v8::Local<v8::Function> interfaceObject, v8::Local<v8::FunctionTemplate> interfaceTemplate) {
#error No one is currently using a partial interface with context-dependent properties.  If you\'re planning to use it, please consult with the binding team: <blink-reviews-bindings@chromium.org>
  V8TestInterface::preparePrototypeAndInterfaceObject(context, world, prototypeObject, interfaceObject, interfaceTemplate);
  v8::Isolate* isolate = context->GetIsolate();
  v8::Local<v8::Name> unscopablesSymbol(v8::Symbol::GetUnscopables(isolate));
  v8::Local<v8::Object> unscopables;
  if (v8CallBoolean(prototypeObject->HasOwnProperty(context, unscopablesSymbol)))
    unscopables = prototypeObject->Get(context, unscopablesSymbol).ToLocalChecked().As<v8::Object>();
  else
    unscopables = v8::Object::New(isolate);
  unscopables->CreateDataProperty(context, v8AtomicString(isolate, "unscopableVoidMethod"), v8::True(isolate)).FromJust();
  prototypeObject->CreateDataProperty(context, unscopablesSymbol, unscopables).FromJust();
}

void V8TestInterfacePartial::initialize() {
  // Should be invoked from ModulesInitializer.
  V8TestInterface::updateWrapperTypeInfo(
      &V8TestInterfacePartial::installV8TestInterfaceTemplate,
      V8TestInterfacePartial::preparePrototypeAndInterfaceObject);
  V8TestInterface::registerVoidMethodPartialOverloadMethodForPartialInterface(&TestInterfaceImplementationPartialV8Internal::voidMethodPartialOverloadMethod);
  V8TestInterface::registerStaticVoidMethodPartialOverloadMethodForPartialInterface(&TestInterfaceImplementationPartialV8Internal::staticVoidMethodPartialOverloadMethod);
  V8TestInterface::registerPromiseMethodPartialOverloadMethodForPartialInterface(&TestInterfaceImplementationPartialV8Internal::promiseMethodPartialOverloadMethod);
  V8TestInterface::registerStaticPromiseMethodPartialOverloadMethodForPartialInterface(&TestInterfaceImplementationPartialV8Internal::staticPromiseMethodPartialOverloadMethod);
  V8TestInterface::registerPartial2VoidMethodMethodForPartialInterface(&TestInterfaceImplementationPartialV8Internal::partial2VoidMethodMethod);
  V8TestInterface::registerPartial2StaticVoidMethodMethodForPartialInterface(&TestInterfaceImplementationPartialV8Internal::partial2StaticVoidMethodMethod);
}

}  // namespace blink
