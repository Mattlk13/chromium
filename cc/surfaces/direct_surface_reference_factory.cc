// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/direct_surface_reference_factory.h"

#include <vector>

#include "cc/surfaces/surface.h"

namespace cc {

DirectSurfaceReferenceFactory::DirectSurfaceReferenceFactory(
    base::WeakPtr<SurfaceManager> manager)
    : manager_(manager) {}

DirectSurfaceReferenceFactory::~DirectSurfaceReferenceFactory() = default;
void DirectSurfaceReferenceFactory::SatisfySequence(
    const SurfaceSequence& sequence) const {
  if (!manager_)
    return;
  std::vector<uint32_t> sequences;
  sequences.push_back(sequence.sequence);
  manager_->DidSatisfySequences(sequence.frame_sink_id, &sequences);
}

void DirectSurfaceReferenceFactory::RequireSequence(
    const SurfaceId& surface_id,
    const SurfaceSequence& sequence) const {
  auto* surface = manager_->GetSurfaceForId(surface_id);
  if (!surface) {
    LOG(ERROR) << "Attempting to require callback on nonexistent surface";
    return;
  }
  surface->AddDestructionDependency(sequence);
}

}  // namespace cc
