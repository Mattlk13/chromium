// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content/client/shell_browser_main_parts.h"

#include "ash/common/login_status.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/content/shell_content_state.h"
#include "ash/shell.h"
#include "ash/shell/content/shell_content_state_impl.h"
#include "ash/shell/example_app_list_presenter.h"
#include "ash/shell/shell_delegate_impl.h"
#include "ash/shell/window_watcher.h"
#include "ash/shell_init_params.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/context_factory.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_net_log.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "net/base/net_module.h"
#include "ui/app_list/presenter/app_list.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/ui_base_paths.h"
#include "ui/compositor/compositor.h"
#include "ui/display/screen.h"
#include "ui/message_center/message_center.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/wm/core/wm_state.h"

#if defined(USE_X11)
#include "ui/events/devices/x11/touch_factory_x11.h"  // nogncheck
#endif

namespace ash {
namespace shell {
void InitWindowTypeLauncher();

namespace {

class ShellViewsDelegate : public views::TestViewsDelegate {
 public:
  ShellViewsDelegate() {}
  ~ShellViewsDelegate() override {}

  // Overridden from views::TestViewsDelegate:
  views::NonClientFrameView* CreateDefaultNonClientFrameView(
      views::Widget* widget) override {
    return ash::Shell::GetInstance()->CreateDefaultNonClientFrameView(widget);
  }
  void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) override {
    if (params->opacity == views::Widget::InitParams::INFER_OPACITY)
      params->opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;

    if (params->native_widget)
      return;

    if (!params->parent && !params->context && !params->child)
      params->context = Shell::GetPrimaryRootWindow();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellViewsDelegate);
};

}  // namespace

ShellBrowserMainParts::ShellBrowserMainParts(
    const content::MainFunctionParams& parameters)
    : BrowserMainParts(), delegate_(nullptr) {}

ShellBrowserMainParts::~ShellBrowserMainParts() {}

void ShellBrowserMainParts::PreMainMessageLoopStart() {
#if defined(USE_X11)
  ui::TouchFactory::SetTouchDeviceListFromCommandLine();
#endif
}

void ShellBrowserMainParts::PostMainMessageLoopStart() {
  chromeos::DBusThreadManager::Initialize(
      chromeos::DBusThreadManager::PROCESS_ASH);
}

void ShellBrowserMainParts::ToolkitInitialized() {
  ash::MaterialDesignController::Initialize();
  wm_state_.reset(new ::wm::WMState);
}

void ShellBrowserMainParts::PreMainMessageLoopRun() {
  net_log_.reset(new content::ShellNetLog("ash_shell"));
  browser_context_.reset(
      new content::ShellBrowserContext(false, net_log_.get()));

  // A ViewsDelegate is required.
  if (!views::ViewsDelegate::GetInstance())
    views_delegate_.reset(new ShellViewsDelegate);

  delegate_ = new ash::shell::ShellDelegateImpl;
  // The global message center state must be initialized absent
  // g_browser_process.
  message_center::MessageCenter::Initialize();

  // Create CrasAudioHandler for testing since g_browser_process
  // is absent.
  chromeos::CrasAudioHandler::InitializeForTesting();

  bluez::BluezDBusManager::Initialize(nullptr, true /* use stub */);

  ShellContentState::SetInstance(
      new ShellContentStateImpl(browser_context_.get()));
  ui::MaterialDesignController::Initialize();
  ash::ShellInitParams init_params;
  init_params.delegate = delegate_;
  init_params.context_factory = content::GetContextFactory();
  init_params.context_factory_private = content::GetContextFactoryPrivate();
  init_params.blocking_pool = content::BrowserThread::GetBlockingPool();
  ash::Shell::CreateInstance(init_params);
  ash::WmShell::Get()->CreateShelf();
  ash::WmShell::Get()->UpdateAfterLoginStatusChange(LoginStatus::USER);

  window_watcher_.reset(new ash::shell::WindowWatcher);
  display::Screen::GetScreen()->AddObserver(window_watcher_.get());

  ash::shell::InitWindowTypeLauncher();

  // Initialize the example app list presenter.
  example_app_list_presenter_ = base::MakeUnique<ExampleAppListPresenter>();
  WmShell::Get()->app_list()->SetAppListPresenter(
      example_app_list_presenter_->CreateInterfacePtrAndBind());

  ash::Shell::GetPrimaryRootWindow()->GetHost()->Show();
}

void ShellBrowserMainParts::PostMainMessageLoopRun() {
  display::Screen::GetScreen()->RemoveObserver(window_watcher_.get());

  window_watcher_.reset();
  delegate_ = nullptr;
  ash::Shell::DeleteInstance();
  ShellContentState::DestroyInstance();
  // The global message center state must be shutdown absent
  // g_browser_process.
  message_center::MessageCenter::Shutdown();

  chromeos::CrasAudioHandler::Shutdown();

  views_delegate_.reset();

  // The keyboard may have created a WebContents. The WebContents is destroyed
  // with the UI, and it needs the BrowserContext to be alive during its
  // destruction. So destroy all of the UI elements before destroying the
  // browser context.
  browser_context_.reset();
}

bool ShellBrowserMainParts::MainMessageLoopRun(int* result_code) {
  base::RunLoop().Run();
  return true;
}

}  // namespace shell
}  // namespace ash
