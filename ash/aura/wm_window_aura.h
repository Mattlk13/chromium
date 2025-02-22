// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AURA_WM_WINDOW_AURA_H_
#define ASH_AURA_WM_WINDOW_AURA_H_

#include "ash/ash_export.h"
#include "ash/common/wm_window.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/aura/window_observer.h"
#include "ui/wm/core/transient_window_observer.h"

namespace ash {

class WmWindowAuraTestApi;

// WmWindowAura is tied to the life of the underlying aura::Window. Use the
// static Get() function to obtain a WmWindowAura from an aura::Window.
class ASH_EXPORT WmWindowAura : public WmWindow,
                                public aura::WindowObserver,
                                public ::wm::TransientWindowObserver {
 public:
  // NOTE: this class is owned by the corresponding window. You shouldn't delete
  // TODO(sky): friend deleter and make private.
  ~WmWindowAura() override;

  // Returns a WmWindow for an aura::Window, creating if necessary. |window| may
  // be null, in which case null is returned.
  static WmWindow* Get(aura::Window* window) {
    return const_cast<WmWindow*>(Get(const_cast<const aura::Window*>(window)));
  }
  static const WmWindow* Get(const aura::Window* window);

  static std::vector<WmWindow*> FromAuraWindows(
      const std::vector<aura::Window*>& aura_windows);
  static std::vector<aura::Window*> ToAuraWindows(
      const std::vector<WmWindow*>& windows);

  static aura::Window* GetAuraWindow(WmWindow* wm_window) {
    return const_cast<aura::Window*>(
        GetAuraWindow(const_cast<const WmWindow*>(wm_window)));
  }
  static const aura::Window* GetAuraWindow(const WmWindow* wm_window);

  aura::Window* aura_window() { return window_; }
  const aura::Window* aura_window() const { return window_; }

  // See description of |children_use_extended_hit_region_|.
  bool ShouldUseExtendedHitRegion() const;

  // WmWindow:
  void Destroy() override;
  const WmWindow* GetRootWindow() const override;
  WmRootWindowController* GetRootWindowController() override;
  WmShell* GetShell() const override;
  void SetName(const char* name) override;
  std::string GetName() const override;
  void SetTitle(const base::string16& title) override;
  base::string16 GetTitle() const override;
  void SetShellWindowId(int id) override;
  int GetShellWindowId() const override;
  WmWindow* GetChildByShellWindowId(int id) override;
  ui::wm::WindowType GetType() const override;
  int GetAppType() const override;
  void SetAppType(int app_type) const override;
  ui::Layer* GetLayer() override;
  bool GetLayerTargetVisibility() override;
  bool GetLayerVisible() override;
  display::Display GetDisplayNearestWindow() override;
  bool HasNonClientArea() override;
  int GetNonClientComponent(const gfx::Point& location) override;
  gfx::Point ConvertPointToTarget(const WmWindow* target,
                                  const gfx::Point& point) const override;
  gfx::Point ConvertPointToScreen(const gfx::Point& point) const override;
  gfx::Point ConvertPointFromScreen(const gfx::Point& point) const override;
  gfx::Rect ConvertRectToScreen(const gfx::Rect& rect) const override;
  gfx::Rect ConvertRectFromScreen(const gfx::Rect& rect) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  bool GetTargetVisibility() const override;
  bool IsVisible() const override;
  void SetOpacity(float opacity) override;
  float GetTargetOpacity() const override;
  gfx::Rect GetMinimizeAnimationTargetBoundsInScreen() const override;
  void SetTransform(const gfx::Transform& transform) override;
  gfx::Transform GetTargetTransform() const override;
  bool IsSystemModal() const override;
  bool GetBoolProperty(WmWindowProperty key) override;
  void SetBoolProperty(WmWindowProperty key, bool value) override;
  SkColor GetColorProperty(WmWindowProperty key) override;
  void SetColorProperty(WmWindowProperty key, SkColor value) override;
  int GetIntProperty(WmWindowProperty key) override;
  void SetIntProperty(WmWindowProperty key, int value) override;
  std::string GetStringProperty(WmWindowProperty key) override;
  void SetStringProperty(WmWindowProperty key,
                         const std::string& value) override;
  gfx::ImageSkia GetWindowIcon() override;
  gfx::ImageSkia GetAppIcon() override;
  const wm::WindowState* GetWindowState() const override;
  WmWindow* GetToplevelWindow() override;
  WmWindow* GetToplevelWindowForFocus() override;
  void SetParentUsingContext(WmWindow* context,
                             const gfx::Rect& screen_bounds) override;
  void AddChild(WmWindow* window) override;
  void RemoveChild(WmWindow* child) override;
  const WmWindow* GetParent() const override;
  const WmWindow* GetTransientParent() const override;
  std::vector<WmWindow*> GetTransientChildren() override;
  bool MoveToEventRoot(const ui::Event& event) override;
  void SetLayoutManager(
      std::unique_ptr<WmLayoutManager> layout_manager) override;
  WmLayoutManager* GetLayoutManager() override;
  void SetVisibilityChangesAnimated() override;
  void SetVisibilityAnimationType(int type) override;
  void SetVisibilityAnimationDuration(base::TimeDelta delta) override;
  void SetVisibilityAnimationTransition(
      ::wm::WindowVisibilityAnimationTransition transition) override;
  void Animate(::wm::WindowAnimationType type) override;
  void StopAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty property) override;
  void SetChildWindowVisibilityChangesAnimated() override;
  void SetMasksToBounds(bool value) override;
  void SetBounds(const gfx::Rect& bounds) override;
  void SetBoundsWithTransitionDelay(const gfx::Rect& bounds,
                                    base::TimeDelta delta) override;
  void SetBoundsDirect(const gfx::Rect& bounds) override;
  void SetBoundsDirectAnimated(const gfx::Rect& bounds) override;
  void SetBoundsDirectCrossFade(const gfx::Rect& bounds) override;
  void SetBoundsInScreen(const gfx::Rect& bounds_in_screen,
                         const display::Display& dst_display) override;
  gfx::Rect GetBoundsInScreen() const override;
  const gfx::Rect& GetBounds() const override;
  gfx::Rect GetTargetBounds() override;
  void ClearRestoreBounds() override;
  void SetRestoreBoundsInScreen(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoreBoundsInScreen() const override;
  bool Contains(const WmWindow* other) const override;
  void SetShowState(ui::WindowShowState show_state) override;
  ui::WindowShowState GetShowState() const override;
  void SetRestoreShowState(ui::WindowShowState show_state) override;
  void SetRestoreOverrides(const gfx::Rect& bounds_override,
                           ui::WindowShowState window_state_override) override;
  void SetLockedToRoot(bool value) override;
  bool IsLockedToRoot() const override;
  void SetCapture() override;
  bool HasCapture() override;
  void ReleaseCapture() override;
  bool HasRestoreBounds() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool CanResize() const override;
  bool CanActivate() const override;
  void StackChildAtTop(WmWindow* child) override;
  void StackChildAtBottom(WmWindow* child) override;
  void StackChildAbove(WmWindow* child, WmWindow* target) override;
  void StackChildBelow(WmWindow* child, WmWindow* target) override;
  void SetPinned(bool trusted) override;
  void SetAlwaysOnTop(bool value) override;
  bool IsAlwaysOnTop() const override;
  void Hide() override;
  void Show() override;
  views::Widget* GetInternalWidget() override;
  void CloseWidget() override;
  void SetFocused() override;
  bool IsFocused() const override;
  bool IsActive() const override;
  void Activate() override;
  void Deactivate() override;
  void SetFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Unminimize() override;
  std::vector<WmWindow*> GetChildren() override;
  void ShowResizeShadow(int component) override;
  void HideResizeShadow() override;
  void InstallResizeHandleWindowTargeter(
      ImmersiveFullscreenController* immersive_fullscreen_controller) override;
  void SetBoundsInScreenBehaviorForChildren(
      BoundsInScreenBehavior behavior) override;
  void SetSnapsChildrenToPhysicalPixelBoundary() override;
  void SnapToPixelBoundaryIfNecessary() override;
  void SetChildrenUseExtendedHitRegion() override;
  std::unique_ptr<views::View> CreateViewWithRecreatedLayers() override;
  void AddObserver(WmWindowObserver* observer) override;
  void RemoveObserver(WmWindowObserver* observer) override;
  bool HasObserver(const WmWindowObserver* observer) const override;
  void AddTransientWindowObserver(WmTransientWindowObserver* observer) override;
  void RemoveTransientWindowObserver(
      WmTransientWindowObserver* observer) override;
  void AddLimitedPreTargetHandler(ui::EventHandler* handler) override;
  void RemoveLimitedPreTargetHandler(ui::EventHandler* handler) override;

 protected:
  explicit WmWindowAura(aura::Window* window);

  // Returns true if a WmWindowAura has been created for |window|.
  static bool HasInstance(const aura::Window* window);

  base::ObserverList<WmWindowObserver>& observers() { return observers_; }

  // aura::WindowObserver:
  void OnWindowHierarchyChanging(const HierarchyChangeParams& params) override;
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowTitleChanged(aura::Window* window) override;

  // ::wm::TransientWindowObserver overrides:
  void OnTransientChildAdded(aura::Window* window,
                             aura::Window* transient) override;
  void OnTransientChildRemoved(aura::Window* window,
                               aura::Window* transient) override;

 private:
  friend class WmWindowAuraTestApi;

  aura::Window* window_;

  base::ObserverList<WmWindowObserver> observers_;

  bool added_transient_observer_ = false;
  base::ObserverList<WmTransientWindowObserver> transient_observers_;

  // If true child windows should get a slightly larger hit region to make
  // resizing easier.
  bool children_use_extended_hit_region_ = false;

  // Default value for |use_empty_minimum_size_for_testing_|.
  static bool default_use_empty_minimum_size_for_testing_;

  // If true the minimum size is 0x0, default is minimum size comes from widget.
  bool use_empty_minimum_size_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(WmWindowAura);
};

}  // namespace ash

#endif  // ASH_AURA_WM_WINDOW_AURA_H_
