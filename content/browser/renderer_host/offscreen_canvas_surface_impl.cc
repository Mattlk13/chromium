// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/offscreen_canvas_surface_impl.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/memory/ptr_util.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/renderer_host/offscreen_canvas_surface_manager.h"
#include "content/public/browser/browser_thread.h"

namespace content {

OffscreenCanvasSurfaceImpl::OffscreenCanvasSurfaceImpl(
    const cc::FrameSinkId& frame_sink_id,
    blink::mojom::OffscreenCanvasSurfaceClientPtr client)
    : client_(std::move(client)), frame_sink_id_(frame_sink_id) {
  OffscreenCanvasSurfaceManager::GetInstance()
      ->RegisterOffscreenCanvasSurfaceInstance(frame_sink_id_, this);
}

OffscreenCanvasSurfaceImpl::~OffscreenCanvasSurfaceImpl() {
  if (frame_sink_id_.is_valid()) {
    OffscreenCanvasSurfaceManager::GetInstance()
        ->UnregisterOffscreenCanvasSurfaceInstance(frame_sink_id_);
  }
}

// static
void OffscreenCanvasSurfaceImpl::Create(
    const cc::FrameSinkId& frame_sink_id,
    blink::mojom::OffscreenCanvasSurfaceClientPtr client,
    blink::mojom::OffscreenCanvasSurfaceRequest request) {
  std::unique_ptr<OffscreenCanvasSurfaceImpl> impl =
      base::MakeUnique<OffscreenCanvasSurfaceImpl>(frame_sink_id,
                                                   std::move(client));
  OffscreenCanvasSurfaceImpl* surface_service = impl.get();
  surface_service->binding_ =
      mojo::MakeStrongBinding(std::move(impl), std::move(request));
}

void OffscreenCanvasSurfaceImpl::OnSurfaceCreated(
    const cc::SurfaceInfo& surface_info) {
  DCHECK_EQ(surface_info.id().frame_sink_id(), frame_sink_id_);
  if (!current_local_frame_id_.is_valid() ||
      surface_info.id().local_frame_id() != current_local_frame_id_) {
    current_local_frame_id_ = surface_info.id().local_frame_id();
    if (client_)
      client_->OnSurfaceCreated(surface_info);
  }
}

void OffscreenCanvasSurfaceImpl::Require(const cc::SurfaceId& surface_id,
                                         const cc::SurfaceSequence& sequence) {
  cc::SurfaceManager* manager = GetSurfaceManager();
  cc::Surface* surface = manager->GetSurfaceForId(surface_id);
  if (!surface) {
    DLOG(ERROR) << "Attempting to require callback on nonexistent surface";
    return;
  }
  surface->AddDestructionDependency(sequence);
}

void OffscreenCanvasSurfaceImpl::Satisfy(const cc::SurfaceSequence& sequence) {
  std::vector<uint32_t> sequences;
  sequences.push_back(sequence.sequence);
  cc::SurfaceManager* manager = GetSurfaceManager();
  manager->DidSatisfySequences(sequence.frame_sink_id, &sequences);
}

}  // namespace content
