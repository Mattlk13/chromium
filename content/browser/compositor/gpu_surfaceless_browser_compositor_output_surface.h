// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_GPU_SURFACELESS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_GPU_SURFACELESS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include <memory>

#include "content/browser/compositor/gpu_browser_compositor_output_surface.h"
#include "gpu/ipc/common/surface_handle.h"

namespace gpu {
class GpuMemoryBufferManager;
}

namespace display_compositor {
class BufferQueue;
class GLHelper;
}

namespace content {

class GpuSurfacelessBrowserCompositorOutputSurface
    : public GpuBrowserCompositorOutputSurface {
 public:
  GpuSurfacelessBrowserCompositorOutputSurface(
      scoped_refptr<ui::ContextProviderCommandBuffer> context,
      gpu::SurfaceHandle surface_handle,
      const UpdateVSyncParametersCallback& update_vsync_parameters_callback,
      std::unique_ptr<display_compositor::CompositorOverlayCandidateValidator>
          overlay_candidate_validator,
      unsigned int target,
      unsigned int internalformat,
      gfx::BufferFormat format,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);
  ~GpuSurfacelessBrowserCompositorOutputSurface() override;

  // cc::OutputSurface implementation.
  void SwapBuffers(cc::OutputSurfaceFrame frame) override;
  void BindFramebuffer() override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha) override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;

  // BrowserCompositorOutputSurface implementation.
  void OnGpuSwapBuffersCompleted(
      const std::vector<ui::LatencyInfo>& latency_info,
      gfx::SwapResult result,
      const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac) override;

 private:
  gfx::Size reshape_size_;
  gfx::Size swap_size_;

  std::unique_ptr<display_compositor::GLHelper> gl_helper_;
  std::unique_ptr<display_compositor::BufferQueue> buffer_queue_;
  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_GPU_SURFACELESS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
