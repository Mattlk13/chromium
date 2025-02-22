// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_RENDER_SURFACE_IMPL_H_
#define CC_LAYERS_RENDER_SURFACE_IMPL_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "cc/base/cc_export.h"
#include "cc/layers/layer_collections.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/shared_quad_state.h"
#include "cc/trees/occlusion.h"
#include "cc/trees/property_tree.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"

namespace cc {

class DamageTracker;
class FilterOperations;
class Occlusion;
class RenderPassSink;
class LayerImpl;
class LayerIterator;
class LayerTreeImpl;

struct AppendQuadsData;

class CC_EXPORT RenderSurfaceImpl {
 public:
  explicit RenderSurfaceImpl(LayerImpl* owning_layer);
  virtual ~RenderSurfaceImpl();

  // Returns the RenderSurfaceImpl that this render surface contributes to. Root
  // render surface's render_target is itself.
  RenderSurfaceImpl* render_target();
  const RenderSurfaceImpl* render_target() const;

  // Returns the rect that encloses the RenderSurfaceImpl including any
  // reflection.
  gfx::RectF DrawableContentRect() const;

  void SetDrawOpacity(float opacity) {
    draw_properties_.draw_opacity = opacity;
  }
  float draw_opacity() const { return draw_properties_.draw_opacity; }

  SkBlendMode BlendMode() const;
  bool UsesDefaultBlendMode() const;

  void SetNearestOcclusionImmuneAncestor(const RenderSurfaceImpl* surface) {
    nearest_occlusion_immune_ancestor_ = surface;
  }
  const RenderSurfaceImpl* nearest_occlusion_immune_ancestor() const {
    return nearest_occlusion_immune_ancestor_;
  }

  SkColor GetDebugBorderColor() const;
  float GetDebugBorderWidth() const;

  void SetDrawTransform(const gfx::Transform& draw_transform) {
    draw_properties_.draw_transform = draw_transform;
  }
  const gfx::Transform& draw_transform() const {
    return draw_properties_.draw_transform;
  }

  void SetScreenSpaceTransform(const gfx::Transform& screen_space_transform) {
    draw_properties_.screen_space_transform = screen_space_transform;
  }
  const gfx::Transform& screen_space_transform() const {
    return draw_properties_.screen_space_transform;
  }

  void SetIsClipped(bool is_clipped) {
    draw_properties_.is_clipped = is_clipped;
  }
  bool is_clipped() const { return draw_properties_.is_clipped; }

  void SetClipRect(const gfx::Rect& clip_rect);
  gfx::Rect clip_rect() const { return draw_properties_.clip_rect; }

  // When false, the RenderSurface does not contribute to another target
  // RenderSurface that is being drawn for the current frame. It could still be
  // drawn to as a target, but its output will not be a part of any other
  // surface.
  bool contributes_to_drawn_surface() const {
    return contributes_to_drawn_surface_;
  }
  void set_contributes_to_drawn_surface(bool contributes_to_drawn_surface) {
    contributes_to_drawn_surface_ = contributes_to_drawn_surface;
  }

  void CalculateContentRectFromAccumulatedContentRect(int max_texture_size);
  void SetContentRectToViewport();
  void SetContentRectForTesting(const gfx::Rect& rect);
  gfx::Rect content_rect() const { return draw_properties_.content_rect; }

  void ClearAccumulatedContentRect();
  void AccumulateContentRectFromContributingLayer(
      LayerImpl* contributing_layer);
  void AccumulateContentRectFromContributingRenderSurface(
      RenderSurfaceImpl* contributing_surface);

  gfx::Rect accumulated_content_rect() const {
    return accumulated_content_rect_;
  }

  const Occlusion& occlusion_in_content_space() const {
    return occlusion_in_content_space_;
  }
  void set_occlusion_in_content_space(const Occlusion& occlusion) {
    occlusion_in_content_space_ = occlusion;
  }

  LayerImplList& layer_list() { return layer_list_; }
  void ClearLayerLists();

  int id() const { return stable_effect_id_; }

  LayerImpl* MaskLayer();
  bool HasMask() const;

  const FilterOperations& Filters() const;
  const FilterOperations& BackgroundFilters() const;
  gfx::PointF FiltersOrigin() const;
  gfx::Transform FiltersTransform() const;

  bool HasCopyRequest() const;

  void ResetPropertyChangedFlags();
  bool SurfacePropertyChanged() const;
  bool SurfacePropertyChangedOnlyFromDescendant() const;
  bool AncestorPropertyChanged() const;
  void NoteAncestorPropertyChanged();

  DamageTracker* damage_tracker() const { return damage_tracker_.get(); }

  int GetRenderPassId();

  void AppendRenderPasses(RenderPassSink* pass_sink);
  void AppendQuads(RenderPass* render_pass, AppendQuadsData* append_quads_data);

  int TransformTreeIndex() const;
  int ClipTreeIndex() const;

  void set_effect_tree_index(int index) { effect_tree_index_ = index; }
  int EffectTreeIndex() const;

 private:
  void SetContentRect(const gfx::Rect& content_rect);
  gfx::Rect CalculateClippedAccumulatedContentRect();

  const EffectNode* OwningEffectNode() const;

  LayerImpl* owning_layer_;

  LayerTreeImpl* layer_tree_impl_;
  int stable_effect_id_;
  int effect_tree_index_;

  // Container for properties that render surfaces need to compute before they
  // can be drawn.
  struct DrawProperties {
    DrawProperties();
    ~DrawProperties();

    float draw_opacity;

    // Transforms from the surface's own space to the space of its target
    // surface.
    gfx::Transform draw_transform;
    // Transforms from the surface's own space to the viewport.
    gfx::Transform screen_space_transform;

    // This is in the surface's own space.
    gfx::Rect content_rect;

    // This is in the space of the surface's target surface.
    gfx::Rect clip_rect;

    // True if the surface needs to be clipped by clip_rect.
    bool is_clipped : 1;
  };

  DrawProperties draw_properties_;

  // Is used to calculate the content rect from property trees.
  gfx::Rect accumulated_content_rect_;
  bool surface_property_changed_ : 1;
  bool ancestor_property_changed_ : 1;

  bool contributes_to_drawn_surface_ : 1;

  LayerImplList layer_list_;
  Occlusion occlusion_in_content_space_;

  // The nearest ancestor target surface that will contain the contents of this
  // surface, and that ignores outside occlusion. This can point to itself.
  const RenderSurfaceImpl* nearest_occlusion_immune_ancestor_;

  std::unique_ptr<DamageTracker> damage_tracker_;

  // For LayerIteratorActions
  int target_render_surface_layer_index_history_;
  size_t current_layer_index_history_;

  friend class LayerIterator;

  DISALLOW_COPY_AND_ASSIGN(RenderSurfaceImpl);
};

}  // namespace cc
#endif  // CC_LAYERS_RENDER_SURFACE_IMPL_H_
