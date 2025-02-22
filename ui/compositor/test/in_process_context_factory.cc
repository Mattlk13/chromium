// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/test/in_process_context_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/output_surface_frame.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/scheduler/delay_based_time_source.h"
#include "cc/surfaces/direct_compositor_frame_sink.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/display_scheduler.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "cc/test/pixel_test_output_surface.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/reflector.h"
#include "ui/compositor/test/in_process_context_provider.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/test/gl_surface_test_support.h"

#if !defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
#include "gpu/ipc/common/gpu_surface_tracker.h"
#endif

namespace ui {
namespace {

class FakeReflector : public Reflector {
 public:
  FakeReflector() {}
  ~FakeReflector() override {}
  void OnMirroringCompositorResized() override {}
  void AddMirroringLayer(Layer* layer) override {}
  void RemoveMirroringLayer(Layer* layer) override {}
};

// An OutputSurface implementation that directly draws and swaps to an actual
// GL surface.
class DirectOutputSurface : public cc::OutputSurface {
 public:
  DirectOutputSurface(scoped_refptr<InProcessContextProvider> context_provider)
      : cc::OutputSurface(std::move(context_provider)),
        weak_ptr_factory_(this) {}

  ~DirectOutputSurface() override {}

  // cc::OutputSurface implementation.
  void BindToClient(cc::OutputSurfaceClient* client) override {
    client_ = client;
  }
  void EnsureBackbuffer() override {}
  void DiscardBackbuffer() override {}
  void BindFramebuffer() override {
    context_provider()->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, 0);
  }
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha) override {
    context_provider()->ContextGL()->ResizeCHROMIUM(
        size.width(), size.height(), device_scale_factor, has_alpha);
  }
  void SwapBuffers(cc::OutputSurfaceFrame frame) override {
    DCHECK(context_provider_.get());
    if (frame.sub_buffer_rect == gfx::Rect(frame.size)) {
      context_provider_->ContextSupport()->Swap();
    } else {
      context_provider_->ContextSupport()->PartialSwapBuffers(
          frame.sub_buffer_rect);
    }
    gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
    const uint64_t fence_sync = gl->InsertFenceSyncCHROMIUM();
    gl->ShallowFlushCHROMIUM();

    gpu::SyncToken sync_token;
    gl->GenUnverifiedSyncTokenCHROMIUM(fence_sync, sync_token.GetData());

    context_provider_->ContextSupport()->SignalSyncToken(
        sync_token, base::Bind(&DirectOutputSurface::OnSwapBuffersComplete,
                               weak_ptr_factory_.GetWeakPtr()));
  }
  uint32_t GetFramebufferCopyTextureFormat() override {
    auto* gl = static_cast<InProcessContextProvider*>(context_provider());
    return gl->GetCopyTextureInternalFormat();
  }
  cc::OverlayCandidateValidator* GetOverlayCandidateValidator() const override {
    return nullptr;
  }
  bool IsDisplayedAsOverlayPlane() const override { return false; }
  unsigned GetOverlayTextureId() const override { return 0; }
  bool SurfaceIsSuspendForRecycle() const override { return false; }
  bool HasExternalStencilTest() const override { return false; }
  void ApplyExternalStencil() override {}

 private:
  void OnSwapBuffersComplete() { client_->DidReceiveSwapBuffersAck(); }

  cc::OutputSurfaceClient* client_ = nullptr;
  base::WeakPtrFactory<DirectOutputSurface> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DirectOutputSurface);
};

}  // namespace

struct InProcessContextFactory::PerCompositorData {
  gpu::SurfaceHandle surface_handle = gpu::kNullSurfaceHandle;
  std::unique_ptr<cc::BeginFrameSource> begin_frame_source;
  std::unique_ptr<cc::Display> display;
};

InProcessContextFactory::InProcessContextFactory(
    bool context_factory_for_test,
    cc::SurfaceManager* surface_manager)
    : next_surface_client_id_(1u),
      use_test_surface_(true),
      context_factory_for_test_(context_factory_for_test),
      surface_manager_(surface_manager) {
  DCHECK(surface_manager);
  DCHECK_NE(gl::GetGLImplementation(), gl::kGLImplementationNone)
      << "If running tests, ensure that main() is calling "
      << "gl::GLSurfaceTestSupport::InitializeOneOff()";
}

InProcessContextFactory::~InProcessContextFactory() {
  DCHECK(per_compositor_data_.empty());
}

void InProcessContextFactory::SendOnLostResources() {
  for (auto& observer : observer_list_)
    observer.OnLostResources();
}

void InProcessContextFactory::CreateCompositorFrameSink(
    base::WeakPtr<Compositor> compositor) {
  // Try to reuse existing shared worker context provider.
  bool shared_worker_context_provider_lost = false;
  if (shared_worker_context_provider_) {
    // Note: If context is lost, delete reference after releasing the lock.
    base::AutoLock lock(*shared_worker_context_provider_->GetLock());
    if (shared_worker_context_provider_->ContextGL()
            ->GetGraphicsResetStatusKHR() != GL_NO_ERROR) {
      shared_worker_context_provider_lost = true;
    }
  }
  if (!shared_worker_context_provider_ || shared_worker_context_provider_lost) {
    shared_worker_context_provider_ = InProcessContextProvider::CreateOffscreen(
        &gpu_memory_buffer_manager_, &image_factory_, nullptr);
    if (shared_worker_context_provider_ &&
        !shared_worker_context_provider_->BindToCurrentThread())
      shared_worker_context_provider_ = nullptr;
  }

  gpu::gles2::ContextCreationAttribHelper attribs;
  attribs.alpha_size = 8;
  attribs.blue_size = 8;
  attribs.green_size = 8;
  attribs.red_size = 8;
  attribs.depth_size = 0;
  attribs.stencil_size = 0;
  attribs.samples = 0;
  attribs.sample_buffers = 0;
  attribs.fail_if_major_perf_caveat = false;
  attribs.bind_generates_resource = false;
  PerCompositorData* data = per_compositor_data_[compositor.get()].get();
  if (!data)
    data = CreatePerCompositorData(compositor.get());

  scoped_refptr<InProcessContextProvider> context_provider =
      InProcessContextProvider::Create(
          attribs, shared_worker_context_provider_.get(),
          &gpu_memory_buffer_manager_, &image_factory_, data->surface_handle,
          "UICompositor");

  std::unique_ptr<cc::OutputSurface> display_output_surface;
  if (use_test_surface_) {
    bool flipped_output_surface = false;
    display_output_surface = base::MakeUnique<cc::PixelTestOutputSurface>(
        context_provider, flipped_output_surface);
  } else {
    display_output_surface =
        base::MakeUnique<DirectOutputSurface>(context_provider);
  }

  std::unique_ptr<cc::DelayBasedBeginFrameSource> begin_frame_source(
      new cc::DelayBasedBeginFrameSource(
          base::MakeUnique<cc::DelayBasedTimeSource>(
              compositor->task_runner().get())));
  std::unique_ptr<cc::DisplayScheduler> scheduler(new cc::DisplayScheduler(
      compositor->task_runner().get(),
      display_output_surface->capabilities().max_frames_pending));

  data->display = base::MakeUnique<cc::Display>(
      &shared_bitmap_manager_, &gpu_memory_buffer_manager_,
      compositor->GetRendererSettings(), compositor->frame_sink_id(),
      begin_frame_source.get(), std::move(display_output_surface),
      std::move(scheduler), base::MakeUnique<cc::TextureMailboxDeleter>(
                                compositor->task_runner().get()));
  // Note that we are careful not to destroy a prior |data->begin_frame_source|
  // until we have reset |data->display|.
  data->begin_frame_source = std::move(begin_frame_source);

  auto* display = per_compositor_data_[compositor.get()]->display.get();
  auto compositor_frame_sink = base::MakeUnique<cc::DirectCompositorFrameSink>(
      compositor->frame_sink_id(), surface_manager_, display, context_provider,
      shared_worker_context_provider_, &gpu_memory_buffer_manager_,
      &shared_bitmap_manager_);
  compositor->SetCompositorFrameSink(std::move(compositor_frame_sink));
}

std::unique_ptr<Reflector> InProcessContextFactory::CreateReflector(
    Compositor* mirrored_compositor,
    Layer* mirroring_layer) {
  return base::WrapUnique(new FakeReflector);
}

void InProcessContextFactory::RemoveReflector(Reflector* reflector) {
}

scoped_refptr<cc::ContextProvider>
InProcessContextFactory::SharedMainThreadContextProvider() {
  if (shared_main_thread_contexts_ &&
      shared_main_thread_contexts_->ContextGL()->GetGraphicsResetStatusKHR() ==
          GL_NO_ERROR)
    return shared_main_thread_contexts_;

  shared_main_thread_contexts_ = InProcessContextProvider::CreateOffscreen(
      &gpu_memory_buffer_manager_, &image_factory_, nullptr);
  if (shared_main_thread_contexts_.get() &&
      !shared_main_thread_contexts_->BindToCurrentThread())
    shared_main_thread_contexts_ = NULL;

  return shared_main_thread_contexts_;
}

void InProcessContextFactory::RemoveCompositor(Compositor* compositor) {
  PerCompositorDataMap::iterator it = per_compositor_data_.find(compositor);
  if (it == per_compositor_data_.end())
    return;
  PerCompositorData* data = it->second.get();
  DCHECK(data);
#if !defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
  if (data->surface_handle)
    gpu::GpuSurfaceTracker::Get()->RemoveSurface(data->surface_handle);
#endif
  per_compositor_data_.erase(it);
}

bool InProcessContextFactory::DoesCreateTestContexts() {
  return context_factory_for_test_;
}

uint32_t InProcessContextFactory::GetImageTextureTarget(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  return GL_TEXTURE_2D;
}

gpu::GpuMemoryBufferManager*
InProcessContextFactory::GetGpuMemoryBufferManager() {
  return &gpu_memory_buffer_manager_;
}

cc::TaskGraphRunner* InProcessContextFactory::GetTaskGraphRunner() {
  return &task_graph_runner_;
}

cc::FrameSinkId InProcessContextFactory::AllocateFrameSinkId() {
  return cc::FrameSinkId(next_surface_client_id_++, 0);
}

cc::SurfaceManager* InProcessContextFactory::GetSurfaceManager() {
  return surface_manager_;
}

void InProcessContextFactory::SetDisplayVisible(ui::Compositor* compositor,
                                                bool visible) {
  if (!per_compositor_data_.count(compositor))
    return;
  per_compositor_data_[compositor]->display->SetVisible(visible);
}

void InProcessContextFactory::ResizeDisplay(ui::Compositor* compositor,
                                            const gfx::Size& size) {
  if (!per_compositor_data_.count(compositor))
    return;
  per_compositor_data_[compositor]->display->Resize(size);
}

void InProcessContextFactory::AddObserver(ContextFactoryObserver* observer) {
  observer_list_.AddObserver(observer);
}

void InProcessContextFactory::RemoveObserver(ContextFactoryObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

InProcessContextFactory::PerCompositorData*
InProcessContextFactory::CreatePerCompositorData(ui::Compositor* compositor) {
  DCHECK(!per_compositor_data_[compositor]);

  gfx::AcceleratedWidget widget = compositor->widget();

  auto data = base::MakeUnique<PerCompositorData>();
  if (widget == gfx::kNullAcceleratedWidget) {
    data->surface_handle = gpu::kNullSurfaceHandle;
  } else {
#if defined(GPU_SURFACE_HANDLE_IS_ACCELERATED_WINDOW)
    data->surface_handle = widget;
#else
    gpu::GpuSurfaceTracker* tracker = gpu::GpuSurfaceTracker::Get();
    data->surface_handle = tracker->AddSurfaceForNativeWidget(widget);
#endif
  }

  PerCompositorData* return_ptr = data.get();
  per_compositor_data_[compositor] = std::move(data);
  return return_ptr;
}

}  // namespace ui
