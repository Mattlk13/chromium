// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_owner.h"

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/test/null_task_runner.h"
#include "cc/animation/animation_player.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {
namespace {

// An animation observer that confirms upon animation completion, that the
// compositor is not null.
class TestLayerAnimationObserver : public ImplicitAnimationObserver {
 public:
  TestLayerAnimationObserver(Layer* layer) : layer_(layer) {}
  ~TestLayerAnimationObserver() override {}

  // ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    EXPECT_NE(nullptr, layer_->GetCompositor());
  }

 private:
  Layer* layer_;

  DISALLOW_COPY_AND_ASSIGN(TestLayerAnimationObserver);
};

class LayerOwnerForTesting : public LayerOwner {
 public:
  void DestroyLayerForTesting() { DestroyLayer(); }
};

// Test fixture for LayerOwner tests that require a ui::Compositor.
class LayerOwnerTestWithCompositor : public testing::Test {
 public:
  LayerOwnerTestWithCompositor();
  ~LayerOwnerTestWithCompositor() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  ui::Compositor* compositor() { return compositor_.get(); }

 private:
  std::unique_ptr<ui::Compositor> compositor_;

  DISALLOW_COPY_AND_ASSIGN(LayerOwnerTestWithCompositor);
};

LayerOwnerTestWithCompositor::LayerOwnerTestWithCompositor() {
}

LayerOwnerTestWithCompositor::~LayerOwnerTestWithCompositor() {
}

void LayerOwnerTestWithCompositor::SetUp() {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      new base::NullTaskRunner();

  ui::ContextFactory* context_factory = nullptr;
  ui::ContextFactoryPrivate* context_factory_private = nullptr;

  ui::InitializeContextFactoryForTests(false, &context_factory,
                                       &context_factory_private);

  compositor_.reset(new ui::Compositor(context_factory, context_factory_private,
                                       task_runner));
  compositor_->SetAcceleratedWidget(gfx::kNullAcceleratedWidget);
}

void LayerOwnerTestWithCompositor::TearDown() {
  compositor_.reset();
  ui::TerminateContextFactoryForTests();
}

}  // namespace

TEST_F(LayerOwnerTestWithCompositor, RecreateRootLayerWithCompositor) {
  LayerOwner owner;
  owner.SetLayer(base::MakeUnique<Layer>());
  Layer* layer = owner.layer();
  compositor()->SetRootLayer(layer);

  std::unique_ptr<Layer> layer_copy = owner.RecreateLayer();

  EXPECT_EQ(compositor(), owner.layer()->GetCompositor());
  EXPECT_EQ(owner.layer(), compositor()->root_layer());
  EXPECT_EQ(nullptr, layer_copy->GetCompositor());
}

// Tests that recreating the root layer, while one of its children is animating,
// properly updates the compositor. So that compositor is not null for observers
// of animations being cancelled.
TEST_F(LayerOwnerTestWithCompositor, RecreateRootLayerDuringAnimation) {
  LayerOwner owner;
  owner.SetLayer(base::MakeUnique<Layer>());
  Layer* layer = owner.layer();
  compositor()->SetRootLayer(layer);

  std::unique_ptr<Layer> child(new Layer);
  child->SetBounds(gfx::Rect(0, 0, 100, 100));
  layer->Add(child.get());

  // This observer checks that the compositor of |child| is not null upon
  // animation completion.
  std::unique_ptr<TestLayerAnimationObserver> observer(
      new TestLayerAnimationObserver(child.get()));
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> long_duration_animation(
      new ui::ScopedAnimationDurationScaleMode(
          ui::ScopedAnimationDurationScaleMode::SLOW_DURATION));
  {
    ui::ScopedLayerAnimationSettings animation(child->GetAnimator());
    animation.SetTransitionDuration(base::TimeDelta::FromMilliseconds(1000));
    animation.AddObserver(observer.get());
    gfx::Transform transform;
    transform.Scale(0.5f, 0.5f);
    child->SetTransform(transform);
  }

  std::unique_ptr<Layer> layer_copy = owner.RecreateLayer();
}

// Tests that recreating a non-root layer, while one of its children is
// animating, properly updates the compositor. So that compositor is not null
// for observers of animations being cancelled.
TEST_F(LayerOwnerTestWithCompositor, RecreateNonRootLayerDuringAnimation) {
  std::unique_ptr<Layer> root_layer(new Layer);
  compositor()->SetRootLayer(root_layer.get());

  LayerOwner owner;
  owner.SetLayer(base::MakeUnique<Layer>());
  Layer* layer = owner.layer();
  root_layer->Add(layer);

  std::unique_ptr<Layer> child(new Layer);
  child->SetBounds(gfx::Rect(0, 0, 100, 100));
  layer->Add(child.get());

  // This observer checks that the compositor of |child| is not null upon
  // animation completion.
  std::unique_ptr<TestLayerAnimationObserver> observer(
      new TestLayerAnimationObserver(child.get()));
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> long_duration_animation(
      new ui::ScopedAnimationDurationScaleMode(
          ui::ScopedAnimationDurationScaleMode::SLOW_DURATION));
  {
    ui::ScopedLayerAnimationSettings animation(child->GetAnimator());
    animation.SetTransitionDuration(base::TimeDelta::FromMilliseconds(1000));
    animation.AddObserver(observer.get());
    gfx::Transform transform;
    transform.Scale(0.5f, 0.5f);
    child->SetTransform(transform);
  }

  std::unique_ptr<Layer> layer_copy = owner.RecreateLayer();
}

// Tests that if LayerOwner-derived class destroys layer, then
// LayerAnimator's player becomes detached from compositor timeline.
TEST_F(LayerOwnerTestWithCompositor, DetachTimelineOnAnimatorDeletion) {
  std::unique_ptr<Layer> root_layer(new Layer);
  compositor()->SetRootLayer(root_layer.get());

  LayerOwnerForTesting owner;
  owner.SetLayer(base::MakeUnique<Layer>());
  Layer* layer = owner.layer();
  layer->SetOpacity(0.5f);
  root_layer->Add(layer);

  scoped_refptr<cc::AnimationPlayer> player =
      layer->GetAnimator()->GetAnimationPlayerForTesting();
  EXPECT_TRUE(player);
  EXPECT_TRUE(player->animation_timeline());

  // Destroying layer/animator must detach animator's player from timeline.
  owner.DestroyLayerForTesting();
  EXPECT_FALSE(player->animation_timeline());
}

// Tests that if we run threaded opacity animation on already added layer
// then LayerAnimator's player becomes attached to timeline.
TEST_F(LayerOwnerTestWithCompositor,
       AttachTimelineIfAnimatorCreatedAfterSetCompositor) {
  std::unique_ptr<Layer> root_layer(new Layer);
  compositor()->SetRootLayer(root_layer.get());

  LayerOwner owner;
  owner.SetLayer(base::MakeUnique<Layer>());
  Layer* layer = owner.layer();
  root_layer->Add(layer);

  layer->SetOpacity(0.5f);

  scoped_refptr<cc::AnimationPlayer> player =
      layer->GetAnimator()->GetAnimationPlayerForTesting();
  EXPECT_TRUE(player);
  EXPECT_TRUE(player->animation_timeline());
}

}  // namespace ui
