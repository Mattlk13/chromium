// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppb_graphics_3d_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/web_preferences.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_instance_throttler_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/ipc/client/command_buffer_proxy_impl.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "ppapi/c/ppp_graphics_3d.h"
#include "ppapi/thunk/enter.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/khronos/GLES2/gl2.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Graphics3D_API;
using blink::WebConsoleMessage;
using blink::WebLocalFrame;
using blink::WebPluginContainer;
using blink::WebString;

namespace content {

PPB_Graphics3D_Impl::PPB_Graphics3D_Impl(PP_Instance instance)
    : PPB_Graphics3D_Shared(instance),
      bound_to_instance_(false),
      commit_pending_(false),
      has_alpha_(false),
      use_image_chromium_(false),
      weak_ptr_factory_(this) {
#if defined(OS_MACOSX)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool use_image_chromium =
      !command_line->HasSwitch(switches::kDisablePepper3DImageChromium);

  if (use_image_chromium) {
    use_image_chromium =
        base::FeatureList::IsEnabled(features::kPepper3DImageChromium);
  }
  use_image_chromium_ = use_image_chromium;
#endif
}

PPB_Graphics3D_Impl::~PPB_Graphics3D_Impl() {
  // Unset the client before the command_buffer_ is destroyed, similar to how
  // WeakPtrFactory invalidates before it.
  if (command_buffer_)
    command_buffer_->SetGpuControlClient(nullptr);
}

// static
PP_Resource PPB_Graphics3D_Impl::CreateRaw(
    PP_Instance instance,
    PP_Resource share_context,
    const gpu::gles2::ContextCreationAttribHelper& attrib_helper,
    gpu::Capabilities* capabilities,
    base::SharedMemoryHandle* shared_state_handle,
    gpu::CommandBufferId* command_buffer_id) {
  PPB_Graphics3D_API* share_api = NULL;
  if (share_context) {
    EnterResourceNoLock<PPB_Graphics3D_API> enter(share_context, true);
    if (enter.failed())
      return 0;
    share_api = enter.object();
  }
  scoped_refptr<PPB_Graphics3D_Impl> graphics_3d(
      new PPB_Graphics3D_Impl(instance));
  if (!graphics_3d->InitRaw(share_api, attrib_helper, capabilities,
                            shared_state_handle, command_buffer_id))
    return 0;
  return graphics_3d->GetReference();
}

PP_Bool PPB_Graphics3D_Impl::SetGetBuffer(int32_t transfer_buffer_id) {
  GetCommandBuffer()->SetGetBuffer(transfer_buffer_id);
  return PP_TRUE;
}

scoped_refptr<gpu::Buffer> PPB_Graphics3D_Impl::CreateTransferBuffer(
    uint32_t size,
    int32_t* id) {
  return GetCommandBuffer()->CreateTransferBuffer(size, id);
}

PP_Bool PPB_Graphics3D_Impl::DestroyTransferBuffer(int32_t id) {
  GetCommandBuffer()->DestroyTransferBuffer(id);
  return PP_TRUE;
}

PP_Bool PPB_Graphics3D_Impl::Flush(int32_t put_offset) {
  GetCommandBuffer()->Flush(put_offset);
  return PP_TRUE;
}

gpu::CommandBuffer::State PPB_Graphics3D_Impl::WaitForTokenInRange(
    int32_t start,
    int32_t end) {
  return GetCommandBuffer()->WaitForTokenInRange(start, end);
}

gpu::CommandBuffer::State PPB_Graphics3D_Impl::WaitForGetOffsetInRange(
    int32_t start,
    int32_t end) {
  return GetCommandBuffer()->WaitForGetOffsetInRange(start, end);
}

void PPB_Graphics3D_Impl::EnsureWorkVisible() {
  command_buffer_->EnsureWorkVisible();
}

void PPB_Graphics3D_Impl::TakeFrontBuffer() {
  if (!taken_front_buffer_.IsZero()) {
    DLOG(ERROR)
        << "TakeFrontBuffer should only be called once before DoSwapBuffers";
    return;
  }
  taken_front_buffer_ = GenerateMailbox();
  command_buffer_->TakeFrontBuffer(taken_front_buffer_);
}

void PPB_Graphics3D_Impl::ReturnFrontBuffer(const gpu::Mailbox& mailbox,
                                            const gpu::SyncToken& sync_token,
                                            bool is_lost) {
  command_buffer_->ReturnFrontBuffer(mailbox, sync_token, is_lost);
}

bool PPB_Graphics3D_Impl::BindToInstance(bool bind) {
  bound_to_instance_ = bind;
  return true;
}

bool PPB_Graphics3D_Impl::IsOpaque() { return !has_alpha_; }

void PPB_Graphics3D_Impl::ViewInitiatedPaint() {
  commit_pending_ = false;

  if (HasPendingSwap())
    SwapBuffersACK(PP_OK);
}

gpu::CommandBufferProxyImpl* PPB_Graphics3D_Impl::GetCommandBufferProxy() {
  DCHECK(command_buffer_);
  return command_buffer_.get();
}

gpu::CommandBuffer* PPB_Graphics3D_Impl::GetCommandBuffer() {
  return command_buffer_.get();
}

gpu::GpuControl* PPB_Graphics3D_Impl::GetGpuControl() {
  return command_buffer_.get();
}

int32_t PPB_Graphics3D_Impl::DoSwapBuffers(const gpu::SyncToken& sync_token,
                                           const gfx::Size& size) {
  DCHECK(command_buffer_);
  if (taken_front_buffer_.IsZero()) {
    DLOG(ERROR) << "TakeFrontBuffer should be called before DoSwapBuffers";
    return PP_ERROR_FAILED;
  }

  if (bound_to_instance_) {
    // If we are bound to the instance, we need to ask the compositor
    // to commit our backing texture so that the graphics appears on the page.
    // When the backing texture will be committed we get notified via
    // ViewFlushedPaint().
    //
    // Don't need to check for NULL from GetPluginInstance since when we're
    // bound, we know our instance is valid.
    bool is_overlay_candidate = use_image_chromium_;
    GLenum target =
        is_overlay_candidate ? GL_TEXTURE_RECTANGLE_ARB : GL_TEXTURE_2D;
    cc::TextureMailbox texture_mailbox(taken_front_buffer_, sync_token, target,
                                       size, is_overlay_candidate, false);
    taken_front_buffer_.SetZero();
    HostGlobals::Get()
        ->GetInstance(pp_instance())
        ->CommitTextureMailbox(texture_mailbox);
    commit_pending_ = true;
  } else {
    // Wait for the command to complete on the GPU to allow for throttling.
    command_buffer_->SignalSyncToken(
        sync_token, base::Bind(&PPB_Graphics3D_Impl::OnSwapBuffers,
                               weak_ptr_factory_.GetWeakPtr()));
  }

  return PP_OK_COMPLETIONPENDING;
}

bool PPB_Graphics3D_Impl::InitRaw(
    PPB_Graphics3D_API* share_context,
    const gpu::gles2::ContextCreationAttribHelper& requested_attribs,
    gpu::Capabilities* capabilities,
    base::SharedMemoryHandle* shared_state_handle,
    gpu::CommandBufferId* command_buffer_id) {
  PepperPluginInstanceImpl* plugin_instance =
      HostGlobals::Get()->GetInstance(pp_instance());
  if (!plugin_instance)
    return false;

  RenderFrame* render_frame = plugin_instance->GetRenderFrame();
  if (!render_frame)
    return false;

  const WebPreferences& prefs = render_frame->GetWebkitPreferences();

  // 3D access might be disabled or blacklisted.
  if (!prefs.pepper_3d_enabled)
    return false;

  // Force SW rendering for keyframe extraction to avoid pixel reads from VRAM.
  PluginInstanceThrottlerImpl* throttler = plugin_instance->throttler();
  if (throttler && throttler->needs_representative_keyframe())
    return false;

  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  if (!render_thread)
    return false;

  scoped_refptr<gpu::GpuChannelHost> channel =
      render_thread->EstablishGpuChannelSync();
  if (!channel)
    return false;

  has_alpha_ = requested_attribs.alpha_size > 0;

  gpu::gles2::ContextCreationAttribHelper attrib_helper = requested_attribs;
  attrib_helper.should_use_native_gmb_for_backbuffer = use_image_chromium_;
  attrib_helper.context_type = gpu::gles2::CONTEXT_TYPE_OPENGLES2;

  gpu::CommandBufferProxyImpl* share_buffer = NULL;
  if (!plugin_instance->is_flash_plugin())
    UMA_HISTOGRAM_BOOLEAN("Pepper.Graphics3DHasShareGroup", !!share_context);
  if (share_context) {
    PPB_Graphics3D_Impl* share_graphics =
        static_cast<PPB_Graphics3D_Impl*>(share_context);
    share_buffer = share_graphics->GetCommandBufferProxy();
  }

  command_buffer_ = gpu::CommandBufferProxyImpl::Create(
      std::move(channel), gpu::kNullSurfaceHandle, share_buffer,
      gpu::GPU_STREAM_DEFAULT, gpu::GpuStreamPriority::NORMAL, attrib_helper,
      GURL::EmptyGURL(), base::ThreadTaskRunnerHandle::Get());
  if (!command_buffer_)
    return false;

  command_buffer_->SetGpuControlClient(this);

  if (shared_state_handle)
    *shared_state_handle = command_buffer_->GetSharedStateHandle();
  if (capabilities)
    *capabilities = command_buffer_->GetCapabilities();
  if (command_buffer_id)
    *command_buffer_id = command_buffer_->GetCommandBufferID();

  return true;
}

void PPB_Graphics3D_Impl::OnGpuControlErrorMessage(const char* message,
                                                   int32_t id) {
  if (!bound_to_instance_)
    return;
  WebPluginContainer* container =
      HostGlobals::Get()->GetInstance(pp_instance())->container();
  if (!container)
    return;
  WebLocalFrame* frame = container->document().frame();
  if (!frame)
    return;
  WebConsoleMessage console_message = WebConsoleMessage(
      WebConsoleMessage::LevelError, WebString::fromUTF8(message));
  frame->addMessageToConsole(console_message);
}

void PPB_Graphics3D_Impl::OnGpuControlLostContext() {
#if DCHECK_IS_ON()
  // This should never occur more than once.
  DCHECK(!lost_context_);
  lost_context_ = true;
#endif

  // Don't need to check for null from GetPluginInstance since when we're
  // bound, we know our instance is valid.
  if (bound_to_instance_) {
    HostGlobals::Get()->GetInstance(pp_instance())->BindGraphics(pp_instance(),
                                                                 0);
  }

  // Send context lost to plugin. This may have been caused by a PPAPI call, so
  // avoid re-entering.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&PPB_Graphics3D_Impl::SendContextLost,
                            weak_ptr_factory_.GetWeakPtr()));
}

void PPB_Graphics3D_Impl::OnGpuControlLostContextMaybeReentrant() {
  // No internal state to update on lost context.
}

void PPB_Graphics3D_Impl::OnSwapBuffers() {
  if (HasPendingSwap()) {
    // If we're off-screen, no need to trigger and wait for compositing.
    // Just send the swap-buffers ACK to the plugin immediately.
    commit_pending_ = false;
    SwapBuffersACK(PP_OK);
  }
}

void PPB_Graphics3D_Impl::SendContextLost() {
  // By the time we run this, the instance may have been deleted, or in the
  // process of being deleted. Even in the latter case, we don't want to send a
  // callback after DidDestroy.
  PepperPluginInstanceImpl* instance =
      HostGlobals::Get()->GetInstance(pp_instance());
  if (!instance || !instance->container())
    return;

  // This PPB_Graphics3D_Impl could be deleted during the call to
  // GetPluginInterface (which sends a sync message in some cases). We still
  // send the Graphics3DContextLost to the plugin; the instance may care about
  // that event even though this context has been destroyed.
  PP_Instance this_pp_instance = pp_instance();
  const PPP_Graphics3D* ppp_graphics_3d = static_cast<const PPP_Graphics3D*>(
      instance->module()->GetPluginInterface(PPP_GRAPHICS_3D_INTERFACE));
  // We have to check *again* that the instance exists, because it could have
  // been deleted during GetPluginInterface(). Even the PluginModule could be
  // deleted, but in that case, the instance should also be gone, so the
  // GetInstance check covers both cases.
  if (ppp_graphics_3d && HostGlobals::Get()->GetInstance(this_pp_instance))
    ppp_graphics_3d->Graphics3DContextLost(this_pp_instance);
}

gpu::Mailbox PPB_Graphics3D_Impl::GenerateMailbox() {
  if (!mailboxes_to_reuse_.empty()) {
    gpu::Mailbox mailbox = mailboxes_to_reuse_.back();
    mailboxes_to_reuse_.pop_back();
    return mailbox;
  }

  return gpu::Mailbox::Generate();
}

}  // namespace content
