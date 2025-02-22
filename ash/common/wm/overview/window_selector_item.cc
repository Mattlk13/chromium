// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/overview/window_selector_item.h"

#include <algorithm>
#include <vector>

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/metrics/user_metrics_action.h"
#include "ash/common/wm/overview/cleanup_animation_observer.h"
#include "ash/common/wm/overview/overview_animation_type.h"
#include "ash/common/wm/overview/scoped_overview_animation_settings.h"
#include "ash/common/wm/overview/scoped_overview_animation_settings_factory.h"
#include "ash/common/wm/overview/scoped_transform_overview_window.h"
#include "ash/common/wm/overview/window_selector.h"
#include "ash/common/wm/overview/window_selector_controller.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_property.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "base/auto_reset.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/transform_util.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/window/non_client_view.h"
#include "ui/wm/core/shadow.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// In the conceptual overview table, the window margin is the space reserved
// around the window within the cell. This margin does not overlap so the
// closest distance between adjacent windows will be twice this amount.
static const int kWindowMargin = 5;

// Cover the transformed window including the gaps between the windows with a
// transparent shield to block the input events from reaching the transformed
// window while in overview.
static const int kWindowSelectorMargin = kWindowMargin * 2;

// Foreground label color.
static const SkColor kLabelColor = SK_ColorWHITE;

// TODO(tdanderson): Move this to a central location.
static const SkColor kCloseButtonColor = SK_ColorWHITE;

// Label background color once in overview mode.
static const SkColor kLabelBackgroundColor = SkColorSetARGB(25, 255, 255, 255);

// Label background color when exiting overview mode.
static const SkColor kLabelExitColor = SkColorSetARGB(255, 90, 90, 90);

// Corner radius for the selection tiles.
static int kLabelBackgroundRadius = 2;

// Vertical padding for the label, on top of it.
static const int kVerticalLabelPadding = 20;

// Horizontal padding for the label, on both sides.
static const int kHorizontalLabelPadding = 8;

// Height of an item header.
static const int kHeaderHeight = 32;

// Opacity for dimmed items.
static const float kDimmedItemOpacity = 0.5f;

// Opacity for fading out during closing a window.
static const float kClosingItemOpacity = 0.8f;

// Opacity for the item header.
static const float kHeaderOpacity =
    (SkColorGetA(kLabelBackgroundColor) / 255.f);

// Duration it takes for the header to shift from opaque header color to
// |kLabelBackgroundColor|.
static const int kSelectorColorSlideMilliseconds = 240;

// Duration of background opacity transition for the selected label.
static const int kSelectorFadeInMilliseconds = 350;

// Duration of background opacity transition when exiting overview mode.
static const int kExitFadeInMilliseconds = 30;

// Before closing a window animate both the window and the caption to shrink by
// this fraction of size.
static const float kPreCloseScale = 0.02f;

// Convenience method to fade in a Window with predefined animation settings.
// Note: The fade in animation will occur after a delay where the delay is how
// long the lay out animations take.
void SetupFadeInAfterLayout(views::Widget* widget) {
  WmWindow* window = WmLookup::Get()->GetWindowForWidget(widget);
  window->SetOpacity(0.0f);
  std::unique_ptr<ScopedOverviewAnimationSettings>
      scoped_overview_animation_settings =
          ScopedOverviewAnimationSettingsFactory::Get()
              ->CreateOverviewAnimationSettings(
                  OverviewAnimationType::
                      OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN,
                  window);
  window->SetOpacity(1.0f);
}

}  // namespace

WindowSelectorItem::OverviewCloseButton::OverviewCloseButton(
    views::ButtonListener* listener)
    : views::ImageButton(listener) {
  icon_image_ = gfx::CreateVectorIcon(gfx::VectorIconId::WINDOW_CONTROL_CLOSE,
                                      kCloseButtonColor);
  SetImage(views::CustomButton::STATE_NORMAL, &icon_image_);
  SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                    views::ImageButton::ALIGN_MIDDLE);
  SetMinimumImageSize(gfx::Size(kHeaderHeight, kHeaderHeight));
}

WindowSelectorItem::OverviewCloseButton::~OverviewCloseButton() {}

// A View having rounded top corners and a specified background color which is
// only painted within the bounds defined by the rounded corners.
// This class coordinates the transitions of the overview mode header when
// entering the overview mode. Those animations are:
// - Opacity animation. The header is initially same color as the original
//   window's header. It starts as transparent and is faded in. When the full
//   opacity is reached the original header is hidden (which is nearly
//   imperceptable because this view obscures the original header) and a color
//   animation starts.
// - Color animation is used to change the color from the opaque color of the
//   original window's header to semi-transparent color of the overview mode
//   header (on entry to overview). It is also used on exit from overview to
//   quickly change the color to a close opaque color in parallel with an
//   opacity transition to mask the original header reappearing.
class WindowSelectorItem::RoundedContainerView
    : public views::View,
      public gfx::AnimationDelegate,
      public ui::LayerAnimationObserver {
 public:
  RoundedContainerView(WindowSelectorItem* item,
                       WmWindow* item_window,
                       int corner_radius,
                       SkColor background)
      : item_(item),
        item_window_(item_window),
        corner_radius_(corner_radius),
        initial_color_(background),
        target_color_(background),
        current_value_(0),
        layer_(nullptr),
        animation_(new gfx::SlideAnimation(this)) {}

  ~RoundedContainerView() override { StopObservingLayerAnimations(); }

  void OnItemRestored() {
    item_ = nullptr;
    item_window_ = nullptr;
  }

  // Starts observing layer animations so that actions can be taken when
  // particular animations (opacity) complete. It should only be called once
  // when the initial fade in animation is started.
  void ObserveLayerAnimations(ui::Layer* layer) {
    DCHECK(!layer_);
    layer_ = layer;
    layer_->GetAnimator()->AddObserver(this);
  }

  // Stops observing layer animations
  void StopObservingLayerAnimations() {
    if (!layer_)
      return;
    layer_->GetAnimator()->RemoveObserver(this);
    layer_ = nullptr;
  }

  // Used by tests to set animation state.
  gfx::SlideAnimation* animation() { return animation_.get(); }

  void set_color(SkColor target_color) { target_color_ = target_color; }

  // Starts a color animation using |tween_type|. The animation will change the
  // color from |initial_color_| to |target_color_| over |duration| specified
  // in milliseconds.
  // This animation can start once the implicit layer fade-in opacity animation
  // is completed. It is used to transition color from the opaque original
  // window header color to |kLabelBackgroundColor| on entry into overview mode
  // and from |kLabelBackgroundColor| back to the original window header color
  // on exit from the overview mode.
  void AnimateColor(gfx::Tween::Type tween_type, int duration) {
    DCHECK(!layer_);  // layer animations should be completed.
    animation_->SetSlideDuration(duration);
    animation_->SetTweenType(tween_type);
    animation_->Reset(0);
    animation_->Show();

    // Tests complete animations immediately. Emulate by invoking the callback.
    if (ui::ScopedAnimationDurationScaleMode::duration_scale_mode() ==
        ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {
      AnimationEnded(animation_.get());
    }
  }

  // Changes the view opacity by animating its background color. The animation
  // will change the alpha value in |target_color_| from its current value to
  // |opacity| * 255 but preserve the RGB values.
  void AnimateBackgroundOpacity(float opacity) {
    animation_->SetSlideDuration(kSelectorFadeInMilliseconds);
    animation_->SetTweenType(gfx::Tween::EASE_OUT);
    animation_->Reset(0);
    animation_->Show();
    target_color_ = SkColorSetA(target_color_, opacity * 255);
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);
    SkScalar radius = SkIntToScalar(corner_radius_);
    const SkScalar kRadius[8] = {radius, radius, radius, radius, 0, 0, 0, 0};
    SkPath path;
    gfx::Rect bounds(size());
    path.addRoundRect(gfx::RectToSkRect(bounds), kRadius);

    SkPaint paint;
    paint.setAntiAlias(true);
    canvas->ClipPath(path, true);

    SkColor target_color = initial_color_;
    if (target_color_ != target_color) {
      target_color = color_utils::AlphaBlend(target_color_, initial_color_,
                                             current_value_);
    }
    canvas->DrawColor(target_color);
  }

 private:
  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override {
    initial_color_ = target_color_;
    // Tabbed browser windows show the overview mode header behind the window
    // during the initial animation. Once the initial fade-in completes and the
    // overview header is fully exposed update stacking to keep the label above
    // the item which prevents input events from reaching the window.
    WmWindow* label_window = WmLookup::Get()->GetWindowForWidget(GetWidget());
    if (label_window && item_window_)
      label_window->GetParent()->StackChildAbove(label_window, item_window_);
    item_window_ = nullptr;
  }

  void AnimationProgressed(const gfx::Animation* animation) override {
    current_value_ = animation_->CurrentValueBetween(0, 255);
    SchedulePaint();
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    item_window_ = nullptr;
    initial_color_ = target_color_;
    current_value_ = 255;
    SchedulePaint();
  }

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {
    if (0 != (sequence->properties() &
              ui::LayerAnimationElement::AnimatableProperty::OPACITY)) {
      if (item_)
        item_->HideHeader();
      StopObservingLayerAnimations();
      AnimateColor(gfx::Tween::EASE_IN, kSelectorColorSlideMilliseconds);
    }
  }

  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {
    if (0 != (sequence->properties() &
              ui::LayerAnimationElement::AnimatableProperty::OPACITY)) {
      StopObservingLayerAnimations();
    }
  }

  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}

  WindowSelectorItem* item_;
  WmWindow* item_window_;
  int corner_radius_;
  SkColor initial_color_;
  SkColor target_color_;
  int current_value_;
  ui::Layer* layer_;
  std::unique_ptr<gfx::SlideAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(RoundedContainerView);
};

WindowSelectorItem::OverviewLabelButton::OverviewLabelButton(
    views::ButtonListener* listener,
    const base::string16& text)
    : LabelButton(listener, text) {}

WindowSelectorItem::OverviewLabelButton::~OverviewLabelButton() {}

void WindowSelectorItem::OverviewLabelButton::SetBackgroundColorHint(
    SkColor color) {
  // Tell the label what color it will be drawn onto. It will use whether the
  // background color is opaque or transparent to decide whether to use subpixel
  // rendering. Does not actually set the label's background color.
  label()->SetBackgroundColor(color);
}

gfx::Rect WindowSelectorItem::OverviewLabelButton::GetChildAreaBounds() {
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(padding_ + gfx::Insets(0, kHorizontalLabelPadding));
  return bounds;
}

// Container View that has an item label and a close button as children.
class WindowSelectorItem::CaptionContainerView : public views::View {
 public:
  CaptionContainerView(WindowSelectorItem::OverviewLabelButton* label,
                       views::ImageButton* close_button,
                       WindowSelectorItem::RoundedContainerView* background)
      : label_(label), close_button_(close_button), background_(background) {
    AddChildView(background_);
    AddChildView(label_);
    AddChildView(close_button_);
  }

 protected:
  // views::View:
  void Layout() override {
    // Position close button in the top right corner sized to its icon size and
    // the label in the top left corner as tall as the button and extending to
    // the button's left edge.
    // The rest of this container view serves as a shield to prevent input
    // events from reaching the transformed window in overview.
    gfx::Rect bounds(GetLocalBounds());
    bounds.Inset(kWindowSelectorMargin, kWindowSelectorMargin);
    gfx::Rect background_bounds(bounds);
    background_bounds.set_height(close_button_->GetPreferredSize().height());
    background_->SetBoundsRect(background_bounds);

    const int visible_height = close_button_->GetPreferredSize().height();
    gfx::Insets label_padding(0, 0, bounds.height() - visible_height,
                              visible_height);
    label_->set_padding(label_padding);
    label_->SetBoundsRect(bounds);
    bounds.set_x(bounds.right() - visible_height);
    bounds.set_width(visible_height);
    bounds.set_height(visible_height);
    close_button_->SetBoundsRect(bounds);
  }

 private:
  WindowSelectorItem::OverviewLabelButton* label_;
  views::ImageButton* close_button_;
  WindowSelectorItem::RoundedContainerView* background_;

  DISALLOW_COPY_AND_ASSIGN(CaptionContainerView);
};

WindowSelectorItem::WindowSelectorItem(WmWindow* window,
                                       WindowSelector* window_selector)
    : dimmed_(false),
      root_window_(window->GetRootWindow()),
      transform_window_(window),
      in_bounds_update_(false),
      selected_(false),
      caption_container_view_(nullptr),
      window_label_button_view_(nullptr),
      close_button_(new OverviewCloseButton(this)),
      window_selector_(window_selector),
      background_view_(nullptr) {
  CreateWindowLabel(window->GetTitle());
  GetWindow()->AddObserver(this);
}

WindowSelectorItem::~WindowSelectorItem() {
  GetWindow()->RemoveObserver(this);
}

WmWindow* WindowSelectorItem::GetWindow() {
  return transform_window_.window();
}

void WindowSelectorItem::RestoreWindow() {
  window_label_button_view_->ResetListener();
  close_button_->ResetListener();
  transform_window_.RestoreWindow();
  if (background_view_) {
    background_view_->OnItemRestored();
    background_view_ = nullptr;
  }
  UpdateHeaderLayout(
      HeaderFadeInMode::EXIT,
      OverviewAnimationType::OVERVIEW_ANIMATION_LAY_OUT_SELECTOR_ITEMS);
}

void WindowSelectorItem::Shutdown() {
  if (transform_window_.GetTopInset()) {
    // Activating a window (even when it is the window that was active before
    // overview) results in stacking it at the top. Maintain the label window
    // stacking position above the item to make the header transformation more
    // gradual upon exiting the overview mode.
    WmWindow* label_window =
        WmLookup::Get()->GetWindowForWidget(window_label_.get());

    // |label_window| was originally created in the same container as the
    // |transform_window_| but when closing overview the |transform_window_|
    // could have been reparented if a drag was active. Only change stacking
    // if the windows still belong to the same container.
    if (label_window->GetParent() == transform_window_.window()->GetParent()) {
      label_window->GetParent()->StackChildAbove(label_window,
                                                 transform_window_.window());
    }
  }
  if (background_view_) {
    background_view_->OnItemRestored();
    background_view_ = nullptr;
  }
  FadeOut(std::move(window_label_));
}

void WindowSelectorItem::PrepareForOverview() {
  transform_window_.PrepareForOverview();
  UpdateHeaderLayout(HeaderFadeInMode::ENTER,
                     OverviewAnimationType::OVERVIEW_ANIMATION_NONE);
}

bool WindowSelectorItem::Contains(const WmWindow* target) const {
  return transform_window_.Contains(target);
}

void WindowSelectorItem::SetBounds(const gfx::Rect& target_bounds,
                                   OverviewAnimationType animation_type) {
  if (in_bounds_update_)
    return;
  base::AutoReset<bool> auto_reset_in_bounds_update(&in_bounds_update_, true);
  target_bounds_ = target_bounds;

  gfx::Rect inset_bounds(target_bounds);
  inset_bounds.Inset(kWindowMargin, kWindowMargin);
  SetItemBounds(inset_bounds, animation_type);

  // SetItemBounds is called before UpdateHeaderLayout so the header can
  // properly use the updated windows bounds.
  UpdateHeaderLayout(HeaderFadeInMode::UPDATE, animation_type);
}

void WindowSelectorItem::SetSelected(bool selected) {
  selected_ = selected;
  background_view_->AnimateBackgroundOpacity(selected ? 0.f : kHeaderOpacity);

  if (shadow_) {
    ui::ScopedLayerAnimationSettings animation_settings_shadow(
        shadow_->shadow_layer()->GetAnimator());
    animation_settings_shadow.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kSelectorFadeInMilliseconds));
    animation_settings_shadow.SetTweenType(
        selected ? gfx::Tween::FAST_OUT_LINEAR_IN
                 : gfx::Tween::LINEAR_OUT_SLOW_IN);
    animation_settings_shadow.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    shadow_->shadow_layer()->SetOpacity(selected ? 0.0f : 1.0f);
  }
}

void WindowSelectorItem::SendAccessibleSelectionEvent() {
  window_label_button_view_->NotifyAccessibilityEvent(ui::AX_EVENT_SELECTION,
                                                      true);
}

void WindowSelectorItem::CloseWindow() {
  gfx::Rect inset_bounds(target_bounds_);
  inset_bounds.Inset(target_bounds_.width() * kPreCloseScale,
                     target_bounds_.height() * kPreCloseScale);
  OverviewAnimationType animation_type =
      OverviewAnimationType::OVERVIEW_ANIMATION_CLOSING_SELECTOR_ITEM;
  // Scale down both the window and label.
  SetBounds(inset_bounds, animation_type);
  // First animate opacity to an intermediate value concurrently with the
  // scaling animation.
  AnimateOpacity(kClosingItemOpacity, animation_type);

  // Fade out the window and the label, effectively hiding them.
  AnimateOpacity(0.0,
                 OverviewAnimationType::OVERVIEW_ANIMATION_CLOSE_SELECTOR_ITEM);
  transform_window_.Close();
}

void WindowSelectorItem::HideHeader() {
  transform_window_.HideHeader();
}

void WindowSelectorItem::OnMinimizedStateChanged() {
  transform_window_.UpdateMirrorWindowForMinimizedState();
}

void WindowSelectorItem::SetDimmed(bool dimmed) {
  dimmed_ = dimmed;
  SetOpacity(dimmed ? kDimmedItemOpacity : 1.0f);
}

void WindowSelectorItem::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (sender == close_button_) {
    WmShell::Get()->RecordUserMetricsAction(UMA_WINDOW_OVERVIEW_CLOSE_BUTTON);
    CloseWindow();
    return;
  }
  CHECK(sender == window_label_button_view_);
  window_selector_->SelectWindow(transform_window_.window());
}

void WindowSelectorItem::OnWindowDestroying(WmWindow* window) {
  window->RemoveObserver(this);
  transform_window_.OnWindowDestroyed();
}

void WindowSelectorItem::OnWindowTitleChanged(WmWindow* window) {
  // TODO(flackr): Maybe add the new title to a vector of titles so that we can
  // filter any of the titles the window had while in the overview session.
  window_label_button_view_->SetText(window->GetTitle());
  UpdateCloseButtonAccessibilityName();
}

float WindowSelectorItem::GetItemScale(const gfx::Size& size) {
  gfx::Size inset_size(size.width(), size.height() - 2 * kWindowMargin);
  return ScopedTransformOverviewWindow::GetItemScale(
      transform_window_.GetTargetBoundsInScreen().size(), inset_size,
      transform_window_.GetTopInset(),
      close_button_->GetPreferredSize().height());
}

gfx::Rect WindowSelectorItem::GetTargetBoundsInScreen() const {
  return transform_window_.GetTargetBoundsInScreen();
}

void WindowSelectorItem::SetItemBounds(const gfx::Rect& target_bounds,
                                       OverviewAnimationType animation_type) {
  DCHECK(root_window_ == GetWindow()->GetRootWindow());
  gfx::Rect screen_rect = transform_window_.GetTargetBoundsInScreen();

  // Avoid division by zero by ensuring screen bounds is not empty.
  gfx::Size screen_size(screen_rect.size());
  screen_size.SetToMax(gfx::Size(1, 1));
  screen_rect.set_size(screen_size);

  const int top_view_inset = transform_window_.GetTopInset();
  const int title_height = close_button_->GetPreferredSize().height();
  gfx::Rect selector_item_bounds =
      ScopedTransformOverviewWindow::ShrinkRectToFitPreservingAspectRatio(
          screen_rect, target_bounds, top_view_inset, title_height);
  gfx::Transform transform = ScopedTransformOverviewWindow::GetTransformForRect(
      screen_rect, selector_item_bounds);
  ScopedTransformOverviewWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  transform_window_.SetTransform(root_window_, transform);
}

void WindowSelectorItem::SetOpacity(float opacity) {
  window_label_->SetOpacity(opacity);
  if (background_view_) {
    background_view_->AnimateBackgroundOpacity(
        selected_ ? 0.f : kHeaderOpacity * opacity);
  }
  transform_window_.SetOpacity(opacity);
}

void WindowSelectorItem::UpdateWindowLabel(
    const gfx::Rect& window_bounds,
    OverviewAnimationType animation_type) {
  if (!window_label_->IsVisible()) {
    window_label_->Show();
    SetupFadeInAfterLayout(window_label_.get());
  }

  gfx::Rect label_bounds = root_window_->ConvertRectFromScreen(window_bounds);
  window_label_button_view_->set_padding(
      gfx::Insets(label_bounds.height() - kVerticalLabelPadding, 0, 0, 0));
  std::unique_ptr<ScopedOverviewAnimationSettings> animation_settings =
      ScopedOverviewAnimationSettingsFactory::Get()
          ->CreateOverviewAnimationSettings(
              animation_type,
              WmLookup::Get()->GetWindowForWidget(window_label_.get()));

  WmWindow* window_label_window =
      WmLookup::Get()->GetWindowForWidget(window_label_.get());
  window_label_window->SetBounds(label_bounds);
}

void WindowSelectorItem::CreateWindowLabel(const base::string16& title) {
  background_view_ = new RoundedContainerView(this, transform_window_.window(),
                                              kLabelBackgroundRadius,
                                              transform_window_.GetTopColor());
  // |background_view_| will get added as a child to CaptionContainerView.
  views::Widget::InitParams params_label;
  params_label.type = views::Widget::InitParams::TYPE_POPUP;
  params_label.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params_label.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params_label.visible_on_all_workspaces = true;
  params_label.name = "OverviewModeLabel";
  params_label.activatable =
      views::Widget::InitParams::Activatable::ACTIVATABLE_DEFAULT;
  params_label.accept_events = true;
  window_label_.reset(new views::Widget);
  root_window_->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(
          window_label_.get(),
          transform_window_.window()->GetParent()->GetShellWindowId(),
          &params_label);
  window_label_->set_focus_on_creation(false);
  window_label_->Init(params_label);
  window_label_button_view_ = new OverviewLabelButton(this, title);
  window_label_button_view_->SetBorder(views::NullBorder());
  window_label_button_view_->SetEnabledTextColors(kLabelColor);
  window_label_button_view_->set_animate_on_state_change(false);
  WmWindow* label_window =
      WmLookup::Get()->GetWindowForWidget(window_label_.get());
  if (transform_window_.GetTopInset()) {
    // For windows with headers the overview header fades in above the
    // original window header.
    label_window->GetParent()->StackChildAbove(label_window,
                                               transform_window_.window());
  } else {
    // For tabbed windows the overview header slides from behind. The stacking
    // is then corrected when the animation completes.
    label_window->GetParent()->StackChildBelow(label_window,
                                               transform_window_.window());
  }
  window_label_button_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // Hint at the background color that the label will be drawn onto (for
  // subpixel antialiasing). Does not actually set the background color.
  window_label_button_view_->SetBackgroundColorHint(kLabelBackgroundColor);
  caption_container_view_ = new CaptionContainerView(
      window_label_button_view_, close_button_, background_view_);
  window_label_->SetContentsView(caption_container_view_);
  window_label_button_view_->SetVisible(false);
  window_label_->SetOpacity(0);
  window_label_->Show();

  // TODO(varkha): Restore shadows when programmatic shadows exist.
  // Note: current shadow implementation does not allow proper animation when
  // the parent layer bounds change during the animation since
  // Shadow::UpdateLayerBounds() only happens before the animation starts.
  if (ash::MaterialDesignController::GetMode() ==
      ash::MaterialDesignController::Mode::MATERIAL_EXPERIMENTAL) {
    shadow_.reset(new ::wm::Shadow());
    shadow_->Init(::wm::ShadowElevation::MEDIUM);
    shadow_->layer()->SetVisible(true);
    window_label_->GetLayer()->Add(shadow_->layer());
  }
  window_label_->GetLayer()->SetMasksToBounds(false);
}

void WindowSelectorItem::UpdateHeaderLayout(
    HeaderFadeInMode mode,
    OverviewAnimationType animation_type) {
  gfx::Rect transformed_window_bounds = root_window_->ConvertRectFromScreen(
      transform_window_.GetTransformedBounds());

  gfx::Rect label_rect(close_button_->GetPreferredSize());
  label_rect.set_width(transformed_window_bounds.width());
  // For tabbed windows the initial bounds of the caption are set such that it
  // appears to be "growing" up from the window content area.
  label_rect.set_y(
      (mode != HeaderFadeInMode::ENTER || transform_window_.GetTopInset())
          ? -label_rect.height()
          : 0);
  if (background_view_) {
    if (mode == HeaderFadeInMode::ENTER) {
      background_view_->ObserveLayerAnimations(window_label_->GetLayer());
      background_view_->set_color(kLabelBackgroundColor);
      // The color will be animated only once the label widget is faded in.
    } else if (mode == HeaderFadeInMode::EXIT) {
      // Normally the observer is disconnected when the fade-in animations
      // complete but some tests invoke animations with |NON_ZERO_DURATION|
      // without waiting for completion so do it here.
      background_view_->StopObservingLayerAnimations();
      // Make the header visible above the window. It will be faded out when
      // the Shutdown() is called.
      background_view_->AnimateColor(gfx::Tween::EASE_OUT,
                                     kExitFadeInMilliseconds);
      background_view_->set_color(kLabelExitColor);
    }
  }
  if (!window_label_button_view_->visible()) {
    window_label_button_view_->SetVisible(true);
    SetupFadeInAfterLayout(window_label_.get());
  }
  WmWindow* window_label_window =
      WmLookup::Get()->GetWindowForWidget(window_label_.get());
  std::unique_ptr<ScopedOverviewAnimationSettings> animation_settings =
      ScopedOverviewAnimationSettingsFactory::Get()
          ->CreateOverviewAnimationSettings(animation_type,
                                            window_label_window);
  // |window_label_window| covers both the transformed window and the header
  // as well as the gap between the windows to prevent events from reaching
  // the window including its sizing borders.
  if (mode != HeaderFadeInMode::ENTER) {
    label_rect.set_height(close_button_->GetPreferredSize().height() +
                          transformed_window_bounds.height());
  }
  label_rect.Inset(-kWindowSelectorMargin, -kWindowSelectorMargin);
  window_label_window->SetBounds(label_rect);
  gfx::Transform label_transform;
  label_transform.Translate(transformed_window_bounds.x(),
                            transformed_window_bounds.y());
  window_label_window->SetTransform(label_transform);

  gfx::Rect shadow_bounds(label_rect.size());
  shadow_bounds.Inset(kWindowSelectorMargin, kWindowSelectorMargin);
  if (shadow_)
    shadow_->SetContentBounds(shadow_bounds);
}

void WindowSelectorItem::AnimateOpacity(float opacity,
                                        OverviewAnimationType animation_type) {
  DCHECK_GE(opacity, 0.f);
  DCHECK_LE(opacity, 1.f);
  ScopedTransformOverviewWindow::ScopedAnimationSettings animation_settings;
  transform_window_.BeginScopedAnimation(animation_type, &animation_settings);
  transform_window_.SetOpacity(opacity);

  const float header_opacity = selected_ ? 0.f : kHeaderOpacity * opacity;
  WmWindow* window_label_window =
      WmLookup::Get()->GetWindowForWidget(window_label_.get());
  std::unique_ptr<ScopedOverviewAnimationSettings> animation_settings_label =
      ScopedOverviewAnimationSettingsFactory::Get()
          ->CreateOverviewAnimationSettings(animation_type,
                                            window_label_window);
  window_label_window->SetOpacity(header_opacity);
}

void WindowSelectorItem::UpdateCloseButtonAccessibilityName() {
  close_button_->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_ASH_OVERVIEW_CLOSE_ITEM_BUTTON_ACCESSIBLE_NAME,
      GetWindow()->GetTitle()));
}

void WindowSelectorItem::FadeOut(std::unique_ptr<views::Widget> widget) {
  widget->SetOpacity(1.f);

  // Fade out the widget. This animation continues past the lifetime of |this|.
  WmWindow* widget_window = WmLookup::Get()->GetWindowForWidget(widget.get());
  std::unique_ptr<ScopedOverviewAnimationSettings> animation_settings =
      ScopedOverviewAnimationSettingsFactory::Get()
          ->CreateOverviewAnimationSettings(
              OverviewAnimationType::
                  OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_FADE_OUT,
              widget_window);
  // CleanupAnimationObserver will delete itself (and the widget) when the
  // opacity animation is complete.
  // Ownership over the observer is passed to the window_selector_->delegate()
  // which has longer lifetime so that animations can continue even after the
  // overview mode is shut down.
  views::Widget* widget_ptr = widget.get();
  std::unique_ptr<CleanupAnimationObserver> observer(
      new CleanupAnimationObserver(std::move(widget)));
  animation_settings->AddObserver(observer.get());
  window_selector_->delegate()->AddDelayedAnimationObserver(
      std::move(observer));
  widget_ptr->SetOpacity(0.f);
}

gfx::SlideAnimation* WindowSelectorItem::GetBackgroundViewAnimation() {
  return background_view_ ? background_view_->animation() : nullptr;
}

WmWindow* WindowSelectorItem::GetOverviewWindowForMinimizedStateForTest() {
  return transform_window_.GetOverviewWindowForMinimizedState();
}

}  // namespace ash
