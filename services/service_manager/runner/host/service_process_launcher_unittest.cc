// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/runner/host/service_process_launcher.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/process_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace service_manager {
namespace {

const char kTestServiceName[] = "host_test_service";

const base::FilePath::CharType kPackagesPath[] = FILE_PATH_LITERAL("Packages");

#if defined(OS_WIN)
const base::FilePath::CharType kServiceExtension[] =
    FILE_PATH_LITERAL(".service.exe");
#else
const base::FilePath::CharType kServiceExtension[] =
    FILE_PATH_LITERAL(".service");
#endif

void ProcessReadyCallbackAdapater(const base::Closure& callback,
                                  base::ProcessId process_id) {
  callback.Run();
}

class ProcessDelegate : public mojo::edk::ProcessDelegate {
 public:
  ProcessDelegate() {}
  ~ProcessDelegate() override {}

 private:
  void OnShutdownComplete() override {}
  DISALLOW_COPY_AND_ASSIGN(ProcessDelegate);
};

class ServiceProcessLauncherDelegateImpl
    : public ServiceProcessLauncher::Delegate {
 public:
  ServiceProcessLauncherDelegateImpl() {}
  ~ServiceProcessLauncherDelegateImpl() override {}

  size_t get_and_clear_adjust_count() {
    size_t count = 0;
    std::swap(count, adjust_count_);
    return count;
  }

 private:
  // ServiceProcessLauncher::Delegate:
  void AdjustCommandLineArgumentsForTarget(
      const Identity& target,
      base::CommandLine* command_line) override {
    adjust_count_++;
  }

  size_t adjust_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ServiceProcessLauncherDelegateImpl);
};

#if defined(OS_ANDROID)
// TODO(qsr): Multiprocess service manager tests are not supported on android.
#define MAYBE_StartJoin DISABLED_StartJoin
#else
#define MAYBE_StartJoin StartJoin
#endif  // defined(OS_ANDROID)
TEST(ServieProcessLauncherTest, MAYBE_StartJoin) {
  base::FilePath service_manager_dir;
  PathService::Get(base::DIR_MODULE, &service_manager_dir);
  base::MessageLoop message_loop;
  scoped_refptr<base::SequencedWorkerPool> blocking_pool(
      new base::SequencedWorkerPool(3, "blocking_pool",
                                    base::TaskPriority::USER_VISIBLE));

  base::Thread io_thread("io_thread");
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  io_thread.StartWithOptions(options);

  ProcessDelegate delegate;
  mojo::edk::InitIPCSupport(&delegate, io_thread.task_runner());

  base::FilePath test_service_path =
      base::FilePath(kPackagesPath).AppendASCII(kTestServiceName)
          .AppendASCII(kTestServiceName) .AddExtension(kServiceExtension);

  ServiceProcessLauncherDelegateImpl service_process_launcher_delegate;
  ServiceProcessLauncher launcher(blocking_pool.get(),
                                  &service_process_launcher_delegate,
                                  test_service_path);
  base::RunLoop run_loop;
  launcher.Start(
      Identity(),
      false,
      base::Bind(&ProcessReadyCallbackAdapater, run_loop.QuitClosure()));
  run_loop.Run();

  launcher.Join();
  blocking_pool->Shutdown();
  mojo::edk::ShutdownIPCSupport();
  EXPECT_EQ(1u, service_process_launcher_delegate.get_and_clear_adjust_count());
}

}  // namespace
}  // namespace service_manager
