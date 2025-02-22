// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/native_extension_bindings_system.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/value_builder.h"
#include "extensions/renderer/api_binding_test.h"
#include "extensions/renderer/api_binding_test_util.h"
#include "extensions/renderer/module_system.h"
#include "extensions/renderer/safe_builtins.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/script_context_set.h"
#include "extensions/renderer/string_source_map.h"
#include "extensions/renderer/test_v8_extension_configuration.h"

namespace extensions {

namespace {

// Creates an extension with the given |name| and |permissions|.
scoped_refptr<Extension> CreateExtension(
    const std::string& name,
    const std::vector<std::string>& permissions) {
  DictionaryBuilder manifest;
  manifest.Set("name", name);
  manifest.Set("manifest_version", 2);
  manifest.Set("version", "0.1");
  manifest.Set("description", "test extension");
  {
    ListBuilder permissions_builder;
    for (const std::string& permission : permissions)
      permissions_builder.Append(permission);
    manifest.Set("permissions", permissions_builder.Build());
  }
  return ExtensionBuilder()
      .SetManifest(manifest.Build())
      .SetLocation(Manifest::INTERNAL)
      .SetID(crx_file::id_util::GenerateId(name))
      .Build();
}

}  // namespace

class NativeExtensionBindingsSystemUnittest : public APIBindingTest {
 public:
  NativeExtensionBindingsSystemUnittest() {}
  ~NativeExtensionBindingsSystemUnittest() override {}

 protected:
  v8::ExtensionConfiguration* GetV8ExtensionConfiguration() override {
    return TestV8ExtensionConfiguration::GetConfiguration();
  }

  void SetUp() override {
    script_context_set_ = base::MakeUnique<ScriptContextSet>(&extension_ids_);
    bindings_system_ = base::MakeUnique<NativeExtensionBindingsSystem>(
        base::Bind(&NativeExtensionBindingsSystemUnittest::MockSendIPC,
                   base::Unretained(this)));
    APIBindingTest::SetUp();
  }

  void TearDown() override {
    for (const auto& context : raw_script_contexts_)
      script_context_set_->Remove(context);
    base::RunLoop().RunUntilIdle();
    script_context_set_.reset();
    bindings_system_.reset();
    APIBindingTest::TearDown();
  }

  void MockSendIPC(ScriptContext* context,
                   const ExtensionHostMsg_Request_Params& params) {
    last_params_.name = params.name;
    last_params_.arguments.Swap(params.arguments.CreateDeepCopy().get());
    last_params_.extension_id = params.extension_id;
    last_params_.source_url = params.source_url;
    last_params_.request_id = params.request_id;
    last_params_.has_callback = params.has_callback;
    last_params_.user_gesture = params.user_gesture;
    last_params_.worker_thread_id = params.worker_thread_id;
    last_params_.service_worker_version_id = params.service_worker_version_id;
  }

  ScriptContext* CreateScriptContext(v8::Local<v8::Context> v8_context,
                                     Extension* extension,
                                     Feature::Context context_type) {
    auto script_context = base::MakeUnique<ScriptContext>(
        v8_context, nullptr, extension, context_type, extension, context_type);
    script_context->set_module_system(
        base::MakeUnique<ModuleSystem>(script_context.get(), source_map()));
    ScriptContext* raw_script_context = script_context.get();
    raw_script_contexts_.push_back(raw_script_context);
    script_context_set_->AddForTesting(std::move(script_context));
    return raw_script_context;
  }

  void RegisterExtension(const ExtensionId& id) { extension_ids_.insert(id); }

  NativeExtensionBindingsSystem* bindings_system() {
    return bindings_system_.get();
  }
  const ExtensionHostMsg_Request_Params& last_params() { return last_params_; }
  StringSourceMap* source_map() { return &source_map_; }

 private:
  ExtensionIdSet extension_ids_;
  std::unique_ptr<ScriptContextSet> script_context_set_;
  std::vector<ScriptContext*> raw_script_contexts_;
  std::unique_ptr<NativeExtensionBindingsSystem> bindings_system_;
  ExtensionHostMsg_Request_Params last_params_;
  StringSourceMap source_map_;

  DISALLOW_COPY_AND_ASSIGN(NativeExtensionBindingsSystemUnittest);
};

TEST_F(NativeExtensionBindingsSystemUnittest, Basic) {
  scoped_refptr<Extension> extension =
      CreateExtension("foo", {"idle", "power"});
  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  // chrome.idle.queryState should exist.
  v8::Local<v8::Value> chrome =
      GetPropertyFromObject(context->Global(), context, "chrome");
  ASSERT_FALSE(chrome.IsEmpty());
  ASSERT_TRUE(chrome->IsObject());

  v8::Local<v8::Value> idle = GetPropertyFromObject(
      v8::Local<v8::Object>::Cast(chrome), context, "idle");
  ASSERT_FALSE(idle.IsEmpty());
  ASSERT_TRUE(idle->IsObject());

  v8::Local<v8::Object> idle_object = v8::Local<v8::Object>::Cast(idle);
  v8::Local<v8::Value> idle_query_state =
      GetPropertyFromObject(idle_object, context, "queryState");
  ASSERT_FALSE(idle_query_state.IsEmpty());

  EXPECT_EQ(ReplaceSingleQuotes(
                "{'ACTIVE':'active','IDLE':'idle','LOCKED':'locked'}"),
            GetStringPropertyFromObject(idle_object, context, "IdleState"));

  {
    // Try calling the function with an invalid invocation - an error should be
    // thrown.
    const char kCallIdleQueryStateInvalid[] =
        "(function() {\n"
        "  chrome.idle.queryState('foo', function(state) {\n"
        "    this.responseState = state;\n"
        "  });\n"
        "});";
    v8::Local<v8::Function> function =
        FunctionFromString(context, kCallIdleQueryStateInvalid);
    ASSERT_FALSE(function.IsEmpty());
    RunFunctionAndExpectError(function, context, 0, nullptr,
                              "Uncaught TypeError: Invalid invocation");
  }

  {
    // Call the function correctly.
    const char kCallIdleQueryState[] =
        "(function() {\n"
        "  chrome.idle.queryState(30, function(state) {\n"
        "    this.responseState = state;\n"
        "  });\n"
        "});";

    v8::Local<v8::Function> call_idle_query_state =
        FunctionFromString(context, kCallIdleQueryState);
    RunFunctionOnGlobal(call_idle_query_state, context, 0, nullptr);
  }

  // Validate the params that would be sent to the browser.
  EXPECT_EQ(extension->id(), last_params().extension_id);
  EXPECT_EQ("idle.queryState", last_params().name);
  EXPECT_EQ(extension->url(), last_params().source_url);
  EXPECT_TRUE(last_params().has_callback);
  EXPECT_TRUE(
      last_params().arguments.Equals(ListValueFromString("[30]").get()));

  // Respond and validate.
  bindings_system()->HandleResponse(last_params().request_id, true,
                                    *ListValueFromString("['active']"),
                                    std::string());

  std::unique_ptr<base::Value> result_value = GetBaseValuePropertyFromObject(
      context->Global(), context, "responseState");
  ASSERT_TRUE(result_value);
  EXPECT_EQ("\"active\"", ValueToString(*result_value));

  // Sanity-check that another API also exists as expected.
  v8::Local<v8::Value> power_api =
      V8ValueFromScriptSource(context, "chrome.power");
  ASSERT_FALSE(power_api.IsEmpty());
  ASSERT_TRUE(power_api->IsObject());
  v8::Local<v8::Value> request_keep_awake = GetPropertyFromObject(
      power_api.As<v8::Object>(), context, "requestKeepAwake");
  ASSERT_FALSE(request_keep_awake.IsEmpty());
  EXPECT_TRUE(request_keep_awake->IsFunction());
}

// Tests that referencing the same API multiple times returns the same object;
// i.e. chrome.foo === chrome.foo.
TEST_F(NativeExtensionBindingsSystemUnittest, APIObjectsAreEqual) {
  scoped_refptr<Extension> extension = CreateExtension("foo", {"idle"});
  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> first_idle_object =
      V8ValueFromScriptSource(context, "chrome.idle");
  ASSERT_FALSE(first_idle_object.IsEmpty());
  EXPECT_TRUE(first_idle_object->IsObject());
  EXPECT_FALSE(first_idle_object->IsUndefined());
  v8::Local<v8::Value> second_idle_object =
      V8ValueFromScriptSource(context, "chrome.idle");
  EXPECT_TRUE(first_idle_object == second_idle_object);
}

// Tests that referencing APIs after the context data is disposed is safe (and
// returns undefined).
TEST_F(NativeExtensionBindingsSystemUnittest,
       ReferencingAPIAfterDisposingContext) {
  scoped_refptr<Extension> extension =
      CreateExtension("foo", {"idle", "power"});

  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  v8::Local<v8::Value> first_idle_object =
      V8ValueFromScriptSource(context, "chrome.idle");
  ASSERT_FALSE(first_idle_object.IsEmpty());
  EXPECT_TRUE(first_idle_object->IsObject());

  DisposeContext();

  // Check an API that was instantiated....
  v8::Local<v8::Value> second_idle_object =
      V8ValueFromScriptSource(context, "chrome.idle");
  ASSERT_FALSE(second_idle_object.IsEmpty());
  EXPECT_TRUE(second_idle_object->IsUndefined());
  // ... and also one that wasn't.
  v8::Local<v8::Value> power_object =
      V8ValueFromScriptSource(context, "chrome.power");
  ASSERT_FALSE(power_object.IsEmpty());
  EXPECT_TRUE(power_object->IsUndefined());
}

// Tests that traditional custom bindings can be used with the native bindings
// system.
TEST_F(NativeExtensionBindingsSystemUnittest, TestBridgingToJSCustomBindings) {
  // Custom binding code. This basically utilizes the interface in binding.js in
  // order to test backwards compatibility.
  const char kCustomBinding[] =
      "apiBridge.registerCustomHook((api, extensionId, contextType) => {\n"
      "  api.apiFunctions.setHandleRequest('queryState',\n"
      "                                    (time, callback) => {\n"
      "    this.timeArg = time;\n"
      "    callback('active');\n"
      "  });\n"
      "  api.apiFunctions.setUpdateArgumentsPreValidate(\n"
      "      'setDetectionInterval', (interval) => {\n"
      "    this.intervalArg = interval;\n"
      "    return [50];\n"
      "  });\n"
      "  this.hookedExtensionId = extensionId;\n"
      "  this.hookedContextType = contextType;\n"
      "  api.compiledApi.hookedApiProperty = 'someProperty';\n"
      "});\n";

  source_map()->RegisterModule("idle", kCustomBinding);

  scoped_refptr<Extension> extension = CreateExtension("foo", {"idle"});
  RegisterExtension(extension->id());

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = ContextLocal();

  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());

  bindings_system()->UpdateBindingsForContext(script_context);

  {
    // Call the function correctly.
    const char kCallIdleQueryState[] =
        "(function() {\n"
        "  chrome.idle.queryState(30, function(state) {\n"
        "    this.responseState = state;\n"
        "  });\n"
        "});";

    v8::Local<v8::Function> call_idle_query_state =
        FunctionFromString(context, kCallIdleQueryState);
    RunFunctionOnGlobal(call_idle_query_state, context, 0, nullptr);
  }

  // To start, check that the properties we set when running the hooks are
  // correct. We do this after calling the function because the API objects (and
  // thus the hooks) are set up lazily.
  v8::Local<v8::Object> global = context->Global();
  EXPECT_EQ(base::StringPrintf("\"%s\"", extension->id().c_str()),
            GetStringPropertyFromObject(global, context, "hookedExtensionId"));
  EXPECT_EQ("\"BLESSED_EXTENSION\"",
            GetStringPropertyFromObject(global, context, "hookedContextType"));
  v8::Local<v8::Value> idle_api =
      V8ValueFromScriptSource(context, "chrome.idle");
  ASSERT_FALSE(idle_api.IsEmpty());
  ASSERT_TRUE(idle_api->IsObject());
  EXPECT_EQ("\"someProperty\"",
            GetStringPropertyFromObject(idle_api.As<v8::Object>(), context,
                                        "hookedApiProperty"));

  // Next, we need to check two pieces: first, that the custom handler was
  // called with the proper arguments....
  EXPECT_EQ("30", GetStringPropertyFromObject(global, context, "timeArg"));

  // ...and second, that the callback was called with the proper result.
  EXPECT_EQ("\"active\"",
            GetStringPropertyFromObject(global, context, "responseState"));

  // Test the updateArgumentsPreValidate hook.
  {
    // Call the function correctly.
    const char kCallIdleSetInterval[] =
        "(function() {\n"
        "  chrome.idle.setDetectionInterval(20);\n"
        "});";

    v8::Local<v8::Function> call_idle_set_interval =
        FunctionFromString(context, kCallIdleSetInterval);
    RunFunctionOnGlobal(call_idle_set_interval, context, 0, nullptr);
  }

  // Since we don't have a custom request handler, the hook should have only
  // updated the arguments. The request then should have gone to the browser
  // normally.
  EXPECT_EQ("20", GetStringPropertyFromObject(global, context, "intervalArg"));
  EXPECT_EQ(extension->id(), last_params().extension_id);
  EXPECT_EQ("idle.setDetectionInterval", last_params().name);
  EXPECT_EQ(extension->url(), last_params().source_url);
  EXPECT_FALSE(last_params().has_callback);
  EXPECT_TRUE(
      last_params().arguments.Equals(ListValueFromString("[50]").get()));
}

}  // namespace extensions
