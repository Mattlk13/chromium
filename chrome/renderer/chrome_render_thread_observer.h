// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_CHROME_RENDER_THREAD_OBSERVER_H_
#define CHROME_RENDERER_CHROME_RENDER_THREAD_OBSERVER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "chrome/common/renderer_configuration.mojom.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/variations/child_process_field_trial_syncer.h"
#include "content/public/renderer/render_thread_observer.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace content {
class ResourceDispatcherDelegate;
}

namespace visitedlink {
class VisitedLinkSlave;
}

// This class filters the incoming control messages (i.e. ones not destined for
// a RenderView) for Chrome specific messages that the content layer doesn't
// happen.  If a few messages are related, they should probably have their own
// observer.
class ChromeRenderThreadObserver : public content::RenderThreadObserver,
                                   public base::FieldTrialList::Observer,
                                   public chrome::mojom::RendererConfiguration {
 public:
  ChromeRenderThreadObserver();
  ~ChromeRenderThreadObserver() override;

  static bool is_incognito_process() { return is_incognito_process_; }

  // Returns a pointer to the content setting rules owned by
  // |ChromeRenderThreadObserver|.
  const RendererContentSettingRules* content_setting_rules() const;

  visitedlink::VisitedLinkSlave* visited_link_slave() {
    return visited_link_slave_.get();
  }

 private:
  // content::RenderThreadObserver:
  void RegisterMojoInterfaces(
      content::AssociatedInterfaceRegistry* associated_interfaces) override;
  void UnregisterMojoInterfaces(
      content::AssociatedInterfaceRegistry* associated_interfaces) override;
  bool OnControlMessageReceived(const IPC::Message& message) override;
  void OnRenderProcessShutdown() override;

  // base::FieldTrialList::Observer:
  void OnFieldTrialGroupFinalized(const std::string& trial_name,
                                  const std::string& group_name) override;

  // chrome::mojom::RendererConfiguration:
  void SetInitialConfiguration(bool is_incognito_process) override;
  void SetContentSettingRules(
      const RendererContentSettingRules& rules) override;

  void OnRendererInterfaceRequest(
      chrome::mojom::RendererConfigurationAssociatedRequest request);

  void OnSetFieldTrialGroup(const std::string& trial_name,
                            const std::string& group_name);

  static bool is_incognito_process_;
  std::unique_ptr<content::ResourceDispatcherDelegate> resource_delegate_;
  RendererContentSettingRules content_setting_rules_;
  variations::ChildProcessFieldTrialSyncer field_trial_syncer_;

  std::unique_ptr<visitedlink::VisitedLinkSlave> visited_link_slave_;

  mojo::AssociatedBinding<chrome::mojom::RendererConfiguration>
      renderer_configuration_binding_;

  base::WeakPtrFactory<ChromeRenderThreadObserver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeRenderThreadObserver);
};

#endif  // CHROME_RENDERER_CHROME_RENDER_THREAD_OBSERVER_H_
