// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/session/tab.h"

#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "blimp/common/create_blimp_message.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "blimp/common/proto/render_widget.pb.h"
#include "blimp/common/proto/tab_control.pb.h"
#include "blimp/engine/common/blimp_user_agent.h"
#include "blimp/engine/feature/engine_render_widget_feature.h"
#include "blimp/net/blimp_message_processor.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/form_field_data.h"
#include "content/public/common/renderer_preferences.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/size.h"

namespace blimp {
namespace engine {

Tab::Tab(std::unique_ptr<content::WebContents> web_contents,
         const int tab_id,
         EngineRenderWidgetFeature* render_widget_feature,
         BlimpMessageProcessor* navigation_message_sender)
    : web_contents_(std::move(web_contents)),
      tab_id_(tab_id),
      render_widget_feature_(render_widget_feature),
      navigation_message_sender_(navigation_message_sender),
      page_load_tracker_(web_contents_.get(), this),
      current_form_request_id_(0),
      weak_factory_(this) {
  DCHECK(render_widget_feature_);
  DCHECK(navigation_message_sender_);

  // A Tab is created upon client's request, thus an updated
  // user agent info (containing client OS info) is available, and we will use
  // that to override user agent string from BlimpContentRendererClient.
  web_contents_->SetUserAgentOverride(GetBlimpEngineUserAgent());

  render_widget_feature_->SetDelegate(tab_id_, this);

  Observe(web_contents_.get());
}

Tab::~Tab() {
  render_widget_feature_->RemoveDelegate(tab_id_);
}

void Tab::Resize(float device_pixel_ratio, const gfx::Size& size_in_dips) {
  DVLOG(1) << "Resize to " << size_in_dips.ToString() << ", "
           << device_pixel_ratio;
  web_contents_->GetNativeView()->SetBounds(gfx::Rect(size_in_dips));

  if (web_contents_->GetRenderViewHost() &&
      web_contents_->GetRenderViewHost()->GetWidget()) {
    web_contents_->GetRenderViewHost()->GetWidget()->WasResized();
  }
}

void Tab::LoadUrl(const GURL& url) {
  TRACE_EVENT1("blimp", "Tab::LoadUrl", "URL", url.spec());
  DVLOG(1) << "Load URL " << url << " in tab " << tab_id_;
  if (!url.is_valid()) {
    VLOG(1) << "Dropping invalid URL " << url;
    return;
  }

  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  params.override_user_agent = content::NavigationController::UA_OVERRIDE_TRUE;
  web_contents_->GetController().LoadURLWithParams(params);
  web_contents_->Focus();
}

void Tab::GoBack() {
  if (!web_contents_->GetController().CanGoBack()) {
    DLOG(ERROR) << "Ignoring back in tab " << tab_id_;
    return;
  }
  DVLOG(1) << "Back in tab " << tab_id_;
  web_contents_->GetController().GoBack();
}

void Tab::GoForward() {
  if (!web_contents_->GetController().CanGoForward()) {
    DLOG(ERROR) << "Ignoring forward in tab " << tab_id_;
    return;
  }
  DVLOG(1) << "Forward in tab " << tab_id_;
  web_contents_->GetController().GoForward();
}

void Tab::Reload() {
  DVLOG(1) << "Reload in tab " << tab_id_;
  web_contents_->GetController().Reload(content::ReloadType::NORMAL, true);
}

void Tab::RenderViewCreated(content::RenderViewHost* render_view_host) {
  DCHECK(render_view_host);
  web_contents_->GetMutableRendererPrefs()->caret_blink_interval = 0;
  render_view_host->SyncRendererPrefs();

  render_widget_feature_->OnRenderWidgetCreated(tab_id_,
                                                render_view_host->GetWidget());
}

void Tab::RenderViewHostChanged(content::RenderViewHost* old_host,
                                content::RenderViewHost* new_host) {
  render_widget_feature_->OnRenderWidgetInitialized(tab_id_,
                                                    new_host->GetWidget());
}

void Tab::RenderViewDeleted(content::RenderViewHost* render_view_host) {
  render_widget_feature_->OnRenderWidgetDeleted(tab_id_,
                                                render_view_host->GetWidget());
}

void Tab::NavigationStateChanged(content::InvalidateTypes changed_flags) {
  DCHECK(changed_flags);

  NavigationMessage* navigation_message;
  std::unique_ptr<BlimpMessage> message =
      CreateBlimpMessage(&navigation_message, tab_id_);
  navigation_message->set_type(NavigationMessage::NAVIGATION_STATE_CHANGED);
  NavigationStateChangeMessage* details =
      navigation_message->mutable_navigation_state_changed();

  if (changed_flags & content::InvalidateTypes::INVALIDATE_TYPE_URL)
    details->set_url(web_contents_->GetURL().spec());

  if (changed_flags & content::InvalidateTypes::INVALIDATE_TYPE_TAB) {
    // TODO(dtrainor): Serialize the favicon? crbug.com/597094.
    DVLOG(3) << "Tab favicon changed";
  }

  if (changed_flags & content::InvalidateTypes::INVALIDATE_TYPE_TITLE)
    details->set_title(base::UTF16ToUTF8(web_contents_->GetTitle()));

  if (changed_flags & content::InvalidateTypes::INVALIDATE_TYPE_LOAD)
    details->set_loading(web_contents_->IsLoading());

  navigation_message_sender_->ProcessMessage(std::move(message),
                                             net::CompletionCallback());
}

void Tab::SendPageLoadStatusUpdate(PageLoadStatus load_status) {
  NavigationMessage* navigation_message = nullptr;
  std::unique_ptr<BlimpMessage> message =
      CreateBlimpMessage(&navigation_message, tab_id_);
  navigation_message->set_type(NavigationMessage::NAVIGATION_STATE_CHANGED);
  navigation_message->mutable_navigation_state_changed()
      ->set_page_load_completed(load_status == PageLoadStatus::LOADED);

  navigation_message_sender_->ProcessMessage(std::move(message),
                                             net::CompletionCallback());
}

void Tab::OnWebGestureEvent(content::RenderWidgetHost* render_widget_host,
                            std::unique_ptr<blink::WebGestureEvent> event) {
  TRACE_EVENT1("blimp", "Tab::OnWebGestureEvent", "type", event->type);
  render_widget_host->ForwardGestureEvent(*event);
}

void Tab::ShowTextInputUI() {
  current_form_request_id_++;
  content::FormFieldDataCallback callback =
      base::Bind(&Tab::ProcessTextInputInfo, weak_factory_.GetWeakPtr(),
                 current_form_request_id_);

  content::RenderFrameHost* focused_frame = web_contents()->GetFocusedFrame();
  if (focused_frame) {
    focused_frame->RequestFocusedFormFieldData(callback);
  }
}

void Tab::HideTextInputUI() {
  current_form_request_id_++;
  render_widget_feature_->SendHideImeRequest(
      tab_id(),
      web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost());
}

void Tab::ProcessTextInputInfo(int request_id,
                               const content::FormFieldData& field) {
  if (field.text_input_type == ui::TEXT_INPUT_TYPE_NONE)
    return;

  // Discard the results for old requests.
  if (request_id < current_form_request_id_) {
    return;
  }

  // TODO(shaktisahu): Remove adding RenderWidgetHost info to the proto.
  render_widget_feature_->SendShowImeRequest(
      tab_id(),
      web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost(), field);
}

void Tab::OnCompositorMessageReceived(
    content::RenderWidgetHost* render_widget_host,
    const std::vector<uint8_t>& message) {
  TRACE_EVENT0("blimp", "Tab::OnCompositorMessageReceived");

  render_widget_host->HandleCompositorProto(message);
}

}  // namespace engine
}  // namespace blimp
