// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/content_renderer_client.h"

#include "cc/blimp/remote_compositor_bridge.h"
#include "content/public/renderer/media_stream_renderer_factory.h"
#include "media/base/renderer_factory.h"
#include "ui/gfx/icc_profile.h"
#include "url/gurl.h"

namespace content {

SkBitmap* ContentRendererClient::GetSadPluginBitmap() {
  return nullptr;
}

SkBitmap* ContentRendererClient::GetSadWebViewBitmap() {
  return nullptr;
}

bool ContentRendererClient::OverrideCreatePlugin(
    RenderFrame* render_frame,
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& params,
    blink::WebPlugin** plugin) {
  return false;
}

blink::WebPlugin* ContentRendererClient::CreatePluginReplacement(
    RenderFrame* render_frame,
    const base::FilePath& plugin_path) {
  return nullptr;
}

bool ContentRendererClient::HasErrorPage(int http_status_code,
                                         std::string* error_domain) {
  return false;
}

bool ContentRendererClient::ShouldSuppressErrorPage(RenderFrame* render_frame,
                                                    const GURL& url) {
  return false;
}

void ContentRendererClient::DeferMediaLoad(
    RenderFrame* render_frame,
    bool has_played_media_before,
    const base::Closure& closure) {
  closure.Run();
}

blink::WebMediaStreamCenter*
ContentRendererClient::OverrideCreateWebMediaStreamCenter(
    blink::WebMediaStreamCenterClient* client) {
  return nullptr;
}

blink::WebRTCPeerConnectionHandler*
ContentRendererClient::OverrideCreateWebRTCPeerConnectionHandler(
    blink::WebRTCPeerConnectionHandlerClient* client) {
  return nullptr;
}

blink::WebMIDIAccessor*
ContentRendererClient::OverrideCreateMIDIAccessor(
    blink::WebMIDIAccessorClient* client) {
  return nullptr;
}

blink::WebAudioDevice*
ContentRendererClient::OverrideCreateAudioDevice(
    double sample_rate) {
  return nullptr;
}

blink::WebClipboard* ContentRendererClient::OverrideWebClipboard() {
  return nullptr;
}

blink::WebThemeEngine* ContentRendererClient::OverrideThemeEngine() {
  return nullptr;
}

blink::WebSpeechSynthesizer* ContentRendererClient::OverrideSpeechSynthesizer(
    blink::WebSpeechSynthesizerClient* client) {
  return nullptr;
}

bool ContentRendererClient::RunIdleHandlerWhenWidgetsHidden() {
  return true;
}

bool ContentRendererClient::AllowTimerSuspensionWhenProcessBackgrounded() {
  return false;
}

bool ContentRendererClient::AllowPopup() {
  return false;
}

#if defined(OS_ANDROID)
bool ContentRendererClient::HandleNavigation(
    RenderFrame* render_frame,
    bool is_content_initiated,
    bool render_view_was_created_by_renderer,
    blink::WebFrame* frame,
    const blink::WebURLRequest& request,
    blink::WebNavigationType type,
    blink::WebNavigationPolicy default_policy,
    bool is_redirect) {
  return false;
}

bool ContentRendererClient::ShouldUseMediaPlayerForURL(const GURL& url) {
  return false;
}
#endif

bool ContentRendererClient::ShouldFork(blink::WebLocalFrame* frame,
                                       const GURL& url,
                                       const std::string& http_method,
                                       bool is_initial_navigation,
                                       bool is_server_redirect,
                                       bool* send_referrer) {
  return false;
}

bool ContentRendererClient::WillSendRequest(blink::WebLocalFrame* frame,
                                            ui::PageTransition transition_type,
                                            const blink::WebURL& url,
                                            GURL* new_url) {
  return false;
}

bool ContentRendererClient::IsPrefetchOnly(
    RenderFrame* render_frame,
    const blink::WebURLRequest& request) {
  return false;
}

unsigned long long ContentRendererClient::VisitedLinkHash(
    const char* canonical_url, size_t length) {
  return 0LL;
}

bool ContentRendererClient::IsLinkVisited(unsigned long long link_hash) {
  return false;
}

blink::WebPrescientNetworking*
ContentRendererClient::GetPrescientNetworking() {
  return nullptr;
}

bool ContentRendererClient::ShouldOverridePageVisibilityState(
    const RenderFrame* render_frame,
    blink::WebPageVisibilityState* override_state) {
  return false;
}

bool ContentRendererClient::IsExternalPepperPlugin(
    const std::string& module_name) {
  return false;
}

bool ContentRendererClient::AllowPepperMediaStreamAPI(const GURL& url) {
  return false;
}

void ContentRendererClient::AddSupportedKeySystems(
    std::vector<std::unique_ptr<media::KeySystemProperties>>* key_systems) {}

std::unique_ptr<MediaStreamRendererFactory>
ContentRendererClient::CreateMediaStreamRendererFactory() {
  return nullptr;
}

cc::ImageSerializationProcessor*
ContentRendererClient::GetImageSerializationProcessor() {
  return nullptr;
}

std::unique_ptr<cc::RemoteCompositorBridge>
ContentRendererClient::CreateRemoteCompositorBridge(
    RemoteProtoChannel* remote_proto_channel,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_main_task_runner) {
  return nullptr;
}

std::unique_ptr<gfx::ICCProfile>
ContentRendererClient::GetImageDecodeColorProfile() {
  return nullptr;
}

bool ContentRendererClient::ShouldReportDetailedMessageForSource(
    const base::string16& source) const {
  return false;
}

bool ContentRendererClient::ShouldGatherSiteIsolationStats() const {
  return true;
}

blink::WebWorkerContentSettingsClientProxy*
ContentRendererClient::CreateWorkerContentSettingsClientProxy(
    RenderFrame* render_frame, blink::WebFrame* frame) {
  return nullptr;
}

bool ContentRendererClient::IsPluginAllowedToUseCameraDeviceAPI(
    const GURL& url) {
  return false;
}

bool ContentRendererClient::IsPluginAllowedToUseCompositorAPI(const GURL& url) {
  return false;
}

bool ContentRendererClient::IsPluginAllowedToUseDevChannelAPIs() {
  return false;
}

BrowserPluginDelegate* ContentRendererClient::CreateBrowserPluginDelegate(
    RenderFrame* render_frame,
    const std::string& mime_type,
    const GURL& original_url) {
  return nullptr;
}

bool ContentRendererClient::ShouldEnforceWebRTCRoutingPreferences() {
  return true;
}

GURL ContentRendererClient::OverrideFlashEmbedWithHTML(const GURL& url) {
  return GURL();
}

}  // namespace content
