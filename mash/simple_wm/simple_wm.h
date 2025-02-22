// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SIMPLE_WM_SIMPLE_WM_H_
#define MASH_SIMPLE_WM_SIMPLE_WM_H_

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/ui/public/cpp/gpu/gpu.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/mus_context_factory.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/property_utils.h"
#include "ui/aura/mus/window_manager_delegate.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/wm_state.h"

namespace display {
class ScreenBase;
}

namespace views {
class AuraInit;
}

namespace wm {
class FocusController;
}

namespace simple_wm {

class SimpleWM : public service_manager::Service,
                 public aura::WindowTreeClientDelegate,
                 public aura::WindowManagerDelegate,
                 public wm::BaseFocusRules {
 public:
  SimpleWM();
  ~SimpleWM() override;

 private:
  class DisplayLayoutManager;
  class FrameView;
  class WindowListModel;
  class WindowListModelObserver;
  class WindowListView;
  class WorkspaceLayoutManager;

  // service_manager::Service:
  void OnStart() override;
  bool OnConnect(const service_manager::ServiceInfo& remote_info,
                 service_manager::InterfaceRegistry* registry) override;

  // aura::WindowTreeClientDelegate:
  void OnEmbed(
      std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) override;
  void OnLostConnection(aura::WindowTreeClient* client) override;
  void OnEmbedRootDestroyed(aura::WindowTreeHostMus* window_tree_host) override;
  void OnPointerEventObserved(const ui::PointerEvent& event,
                              aura::Window* target) override;
  aura::client::CaptureClient* GetCaptureClient() override;
  aura::PropertyConverter* GetPropertyConverter() override;

  // aura::WindowManagerDelegate:
  void SetWindowManagerClient(aura::WindowManagerClient* client) override;
  bool OnWmSetBounds(aura::Window* window, gfx::Rect* bounds) override;
  bool OnWmSetProperty(
      aura::Window* window,
      const std::string& name,
      std::unique_ptr<std::vector<uint8_t>>* new_data) override;
  aura::Window* OnWmCreateTopLevelWindow(
      ui::mojom::WindowType window_type,
      std::map<std::string, std::vector<uint8_t>>* properties) override;
  void OnWmClientJankinessChanged(const std::set<aura::Window*>& client_windows,
                                  bool janky) override;
  void OnWmWillCreateDisplay(const display::Display& display) override;
  void OnWmNewDisplay(std::unique_ptr<aura::WindowTreeHostMus> window_tree_host,
                      const display::Display& display) override;
  void OnWmDisplayRemoved(aura::WindowTreeHostMus* window_tree_host) override;
  void OnWmDisplayModified(const display::Display& display) override;
  void OnWmPerformMoveLoop(aura::Window* window,
                           ui::mojom::MoveLoopSource source,
                           const gfx::Point& cursor_location,
                           const base::Callback<void(bool)>& on_done) override;
  void OnWmCancelMoveLoop(aura::Window* window) override;
  void OnWmSetClientArea(
      aura::Window* window,
      const gfx::Insets& insets,
      const std::vector<gfx::Rect>& additional_client_areas) override;

  // wm::BaseFocusRules:
  bool SupportsChildActivation(aura::Window* window) const override;
  bool IsWindowConsideredVisibleForActivation(
      aura::Window* window) const override;

  FrameView* GetFrameViewForClientWindow(aura::Window* client_window);

  void OnWindowListViewItemActivated(aura::Window* index);

  std::unique_ptr<views::AuraInit> aura_init_;
  wm::WMState wm_state_;
  std::unique_ptr<display::ScreenBase> screen_;
  aura::PropertyConverter property_converter_;
  std::unique_ptr<wm::FocusController> focus_controller_;
  std::unique_ptr<aura::WindowTreeHostMus> window_tree_host_;
  aura::Window* display_root_ = nullptr;
  aura::Window* window_root_ = nullptr;
  aura::WindowManagerClient* window_manager_client_ = nullptr;
  std::unique_ptr<aura::WindowTreeClient> window_tree_client_;
  std::unique_ptr<ui::Gpu> gpu_;
  std::unique_ptr<aura::MusContextFactory> compositor_context_factory_;
  std::map<aura::Window*, FrameView*> client_window_to_frame_view_;
  std::unique_ptr<WindowListModel> window_list_model_;
  std::unique_ptr<WorkspaceLayoutManager> workspace_layout_manager_;

  bool started_ = false;

  DISALLOW_COPY_AND_ASSIGN(SimpleWM);
};

}  // namespace simple_wm

#endif  // MASH_SIMPLE_WM_SIMPLE_WM_H_
