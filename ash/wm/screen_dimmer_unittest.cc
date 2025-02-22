// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/wm/screen_dimmer.h"

#include <memory>

#include "ash/aura/wm_window_aura.h"
#include "ash/common/wm/window_dimmer.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window_user_data.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/test/test_windows.h"
#include "ui/compositor/layer.h"

namespace ash {
namespace test {

class ScreenDimmerTest : public AshTestBase {
 public:
  ScreenDimmerTest() {}
  ~ScreenDimmerTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    dimmer_ = base::MakeUnique<ScreenDimmer>(ScreenDimmer::Container::ROOT);
  }

  void TearDown() override {
    dimmer_.reset();
    AshTestBase::TearDown();
  }

  aura::Window* GetDimWindow() {
    WindowDimmer* window_dimmer =
        dimmer_->window_dimmers_->Get(WmShell::Get()->GetPrimaryRootWindow());
    return window_dimmer ? WmWindowAura::GetAuraWindow(window_dimmer->window())
                         : nullptr;
  }

  ui::Layer* GetDimWindowLayer() {
    aura::Window* window = GetDimWindow();
    return window ? window->layer() : nullptr;
  }

 protected:
  std::unique_ptr<ScreenDimmer> dimmer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenDimmerTest);
};

TEST_F(ScreenDimmerTest, DimAndUndim) {
  // Don't create a layer until we need to.
  EXPECT_EQ(nullptr, GetDimWindowLayer());
  dimmer_->SetDimming(false);
  EXPECT_EQ(nullptr, GetDimWindowLayer());

  // When we enable dimming, the layer should be created and stacked at the top
  // of the root's children.
  dimmer_->SetDimming(true);
  ASSERT_NE(nullptr, GetDimWindowLayer());
  ui::Layer* root_layer = Shell::GetPrimaryRootWindow()->layer();
  ASSERT_TRUE(!root_layer->children().empty());
  EXPECT_EQ(GetDimWindowLayer(), root_layer->children().back());
  EXPECT_TRUE(GetDimWindowLayer()->visible());
  EXPECT_GT(GetDimWindowLayer()->GetTargetOpacity(), 0.0f);

  // When we disable dimming, the layer should be removed.
  dimmer_->SetDimming(false);
  ASSERT_EQ(nullptr, GetDimWindowLayer());
}

TEST_F(ScreenDimmerTest, ResizeLayer) {
  // The dimming layer should be initially sized to cover the root window.
  dimmer_->SetDimming(true);
  ui::Layer* dimming_layer = GetDimWindowLayer();
  ASSERT_TRUE(dimming_layer != nullptr);
  ui::Layer* root_layer = Shell::GetPrimaryRootWindow()->layer();
  EXPECT_EQ(gfx::Rect(root_layer->bounds().size()).ToString(),
            dimming_layer->bounds().ToString());

  // When we resize the root window, the dimming layer should be resized to
  // match.
  gfx::Rect kNewBounds(400, 300);
  Shell::GetPrimaryRootWindow()->GetHost()->SetBoundsInPixels(kNewBounds);
  EXPECT_EQ(kNewBounds.ToString(), dimming_layer->bounds().ToString());
}

TEST_F(ScreenDimmerTest, DimAtBottom) {
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithId(1, root_window));
  dimmer_->SetDimming(true);
  std::vector<aura::Window*>::const_iterator dim_iter =
      std::find(root_window->children().begin(), root_window->children().end(),
                GetDimWindow());
  ASSERT_TRUE(dim_iter != root_window->children().end());
  // Dim layer is at top.
  EXPECT_EQ(*dim_iter, *root_window->children().rbegin());

  dimmer_->SetDimming(false);
  dimmer_->set_at_bottom(true);
  dimmer_->SetDimming(true);

  dim_iter = std::find(root_window->children().begin(),
                       root_window->children().end(), GetDimWindow());
  ASSERT_TRUE(dim_iter != root_window->children().end());
  // Dom layer is at the bottom.
  EXPECT_EQ(*dim_iter, *root_window->children().begin());
}

// See description above TEST_F for details.
class ScreenDimmerShellDestructionTest : public AshTestBase {
 public:
  ScreenDimmerShellDestructionTest() {}
  ~ScreenDimmerShellDestructionTest() override {}

  void TearDown() override {
    ScreenDimmer screen_dimmer(ScreenDimmer::Container::ROOT);
    AshTestBase::TearDown();
    // ScreenDimmer is destroyed *after* the shell.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenDimmerShellDestructionTest);
};

// This test verifies ScreenDimmer can be destroyed after the shell. The
// interesting part of this test is in TearDown(), which creates a ScreenDimmer
// that is deleted after WmShell.
TEST_F(ScreenDimmerShellDestructionTest, DontCrashIfScreenDimmerOutlivesShell) {
}

}  // namespace test
}  // namespace ash
