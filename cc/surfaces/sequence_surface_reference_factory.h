// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_SEQUENCE_SURFACE_REFERENCE_FACTORY_H_
#define CC_SURFACES_SEQUENCE_SURFACE_REFERENCE_FACTORY_H_

#include "cc/output/compositor_frame_metadata.h"
#include "cc/surfaces/surface_reference_base.h"
#include "cc/surfaces/surface_reference_factory.h"
#include "cc/surfaces/surfaces_export.h"

namespace cc {

// A surface reference factory that generates references which internally
// use SurfaceSequence.
class CC_SURFACES_EXPORT SequenceSurfaceReferenceFactory
    : public NON_EXPORTED_BASE(SurfaceReferenceFactory) {
 public:
  SequenceSurfaceReferenceFactory() = default;

  std::unique_ptr<SurfaceReferenceBase> CreateReference(
      SurfaceReferenceOwner* owner,
      const SurfaceId& surface_id) const override;

 protected:
  ~SequenceSurfaceReferenceFactory() override = default;

 private:
  virtual void RequireSequence(const SurfaceId& surface_id,
                               const SurfaceSequence& sequence) const = 0;
  virtual void SatisfySequence(const SurfaceSequence& sequence) const = 0;

  // SurfaceReferenceFactory implementation:
  void DestroyReference(SurfaceReferenceBase* surface_ref) const override;

  DISALLOW_COPY_AND_ASSIGN(SequenceSurfaceReferenceFactory);
};

}  // namespace cc

#endif  // CC_SURFACES_SEQUENCE_SURFACE_REFERENCE_FACTORY_H_
