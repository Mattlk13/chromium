// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/test/mash_test_suite.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "ui/aura/env.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/test/gl_surface_test_support.h"

namespace mash {
namespace test {

MashTestSuite::MashTestSuite(int argc, char** argv) : TestSuite(argc, argv) {}

MashTestSuite::~MashTestSuite() {}

void MashTestSuite::Initialize() {
  base::TestSuite::Initialize();

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kOverrideUseGLWithOSMesaForTests);

  // Load ash mus strings and resources; not 'common' (Chrome) resources.
  base::FilePath resources;
  PathService::Get(base::DIR_MODULE, &resources);
  resources = resources.Append(FILE_PATH_LITERAL("ash_mus_resources.pak"));
  ui::ResourceBundle::InitSharedInstanceWithPakPath(resources);

  base::DiscardableMemoryAllocator::SetInstance(&discardable_memory_allocator_);
  env_ = aura::Env::CreateInstance(aura::Env::Mode::MUS);
  gl::GLSurfaceTestSupport::InitializeOneOff();
  const bool enable_pixel_output = false;

  ui::ContextFactory* context_factory = nullptr;
  ui::ContextFactoryPrivate* context_factory_private = nullptr;
  ui::InitializeContextFactoryForTests(enable_pixel_output, &context_factory,
                                       &context_factory_private);

  env_->set_context_factory(context_factory);
  env_->set_context_factory_private(context_factory_private);
}

void MashTestSuite::Shutdown() {
  ui::TerminateContextFactoryForTests();
  env_.reset();
  ui::ResourceBundle::CleanupSharedInstance();
  base::TestSuite::Shutdown();
}

}  // namespace test
}  // namespace mash
