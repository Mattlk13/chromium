// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_UI_BINDINGS_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_UI_BINDINGS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "chrome/browser/devtools/devtools_embedder_message_dispatcher.h"
#include "chrome/browser/devtools/devtools_file_helper.h"
#include "chrome/browser/devtools/devtools_file_system_indexer.h"
#include "chrome/browser/devtools/devtools_targets_ui.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "ui/gfx/geometry/size.h"

class DevToolsAndroidBridge;
class InfoBarService;
class Profile;
class PortForwardingStatusSerializer;

namespace content {
class WebContents;
}

// Base implementation of DevTools bindings around front-end.
class DevToolsUIBindings : public DevToolsEmbedderMessageDispatcher::Delegate,
                           public DevToolsAndroidBridge::DeviceCountListener,
                           public content::DevToolsAgentHostClient,
                           public net::URLFetcherDelegate,
                           public DevToolsFileHelper::Delegate {
 public:
  static DevToolsUIBindings* ForWebContents(
      content::WebContents* web_contents);

  static GURL SanitizeFrontendURL(const GURL& url);

  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void ActivateWindow() = 0;
    virtual void CloseWindow() = 0;
    virtual void Inspect(scoped_refptr<content::DevToolsAgentHost> host) = 0;
    virtual void SetInspectedPageBounds(const gfx::Rect& rect) = 0;
    virtual void InspectElementCompleted() = 0;
    virtual void SetIsDocked(bool is_docked) = 0;
    virtual void OpenInNewTab(const std::string& url) = 0;
    virtual void SetWhitelistedShortcuts(const std::string& message) = 0;

    virtual void InspectedContentsClosing() = 0;
    virtual void OnLoadCompleted() = 0;
    virtual void ReadyForTest() = 0;
    virtual InfoBarService* GetInfoBarService() = 0;
    virtual void RenderProcessGone(bool crashed) = 0;
  };

  explicit DevToolsUIBindings(content::WebContents* web_contents);
  ~DevToolsUIBindings() override;

  content::WebContents* web_contents() { return web_contents_; }
  Profile* profile() { return profile_; }
  content::DevToolsAgentHost* agent_host() { return agent_host_.get(); }

  // Takes ownership over the |delegate|.
  void SetDelegate(Delegate* delegate);
  void CallClientFunction(const std::string& function_name,
                          const base::Value* arg1,
                          const base::Value* arg2,
                          const base::Value* arg3);
  void AttachTo(const scoped_refptr<content::DevToolsAgentHost>& agent_host);
  void Reload();
  void Detach();
  bool IsAttachedTo(content::DevToolsAgentHost* agent_host);

 private:
  void HandleMessageFromDevToolsFrontend(const std::string& message);

  // content::DevToolsAgentHostClient implementation.
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               const std::string& message) override;
  void AgentHostClosed(content::DevToolsAgentHost* agent_host,
                       bool replaced_with_another_client) override;

  // DevToolsEmbedderMessageDispatcher::Delegate implementation.
  void ActivateWindow() override;
  void CloseWindow() override;
  void LoadCompleted() override;
  void SetInspectedPageBounds(const gfx::Rect& rect) override;
  void InspectElementCompleted() override;
  void InspectedURLChanged(const std::string& url) override;
  void LoadNetworkResource(const DispatchCallback& callback,
                           const std::string& url,
                           const std::string& headers,
                           int stream_id) override;
  void SetIsDocked(const DispatchCallback& callback, bool is_docked) override;
  void OpenInNewTab(const std::string& url) override;
  void SaveToFile(const std::string& url,
                  const std::string& content,
                  bool save_as) override;
  void AppendToFile(const std::string& url,
                    const std::string& content) override;
  void RequestFileSystems() override;
  void AddFileSystem(const std::string& file_system_path) override;
  void RemoveFileSystem(const std::string& file_system_path) override;
  void UpgradeDraggedFileSystemPermissions(
      const std::string& file_system_url) override;
  void IndexPath(int index_request_id,
                 const std::string& file_system_path) override;
  void StopIndexing(int index_request_id) override;
  void SearchInPath(int search_request_id,
                    const std::string& file_system_path,
                    const std::string& query) override;
  void SetWhitelistedShortcuts(const std::string& message) override;
  void ShowCertificateViewer(const std::string& cert_chain) override;
  void ZoomIn() override;
  void ZoomOut() override;
  void ResetZoom() override;
  void SetDevicesDiscoveryConfig(
      bool discover_usb_devices,
      bool port_forwarding_enabled,
      const std::string& port_forwarding_config) override;
  void SetDevicesUpdatesEnabled(bool enabled) override;
  void PerformActionOnRemotePage(const std::string& page_id,
                                 const std::string& action) override;
  void OpenRemotePage(const std::string& browser_id,
                      const std::string& url) override;
  void DispatchProtocolMessageFromDevToolsFrontend(
      const std::string& message) override;
  void RecordEnumeratedHistogram(const std::string& name,
                                 int sample,
                                 int boundary_value) override;
  void SendJsonRequest(const DispatchCallback& callback,
                       const std::string& browser_id,
                       const std::string& url) override;
  void GetPreferences(const DispatchCallback& callback) override;
  void SetPreference(const std::string& name,
                     const std::string& value) override;
  void RemovePreference(const std::string& name) override;
  void ClearPreferences() override;
  void Reattach(const DispatchCallback& callback) override;
  void ReadyForTest() override;

  // net::URLFetcherDelegate overrides.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  void EnableRemoteDeviceCounter(bool enable);

  void SendMessageAck(int request_id,
                      const base::Value* arg1);

  // DevToolsAndroidBridge::DeviceCountListener override:
  void DeviceCountChanged(int count) override;

  // Forwards discovered devices to frontend.
  virtual void DevicesUpdated(const std::string& source,
                              const base::ListValue& targets);

  void DocumentAvailableInMainFrame();
  void DocumentOnLoadCompletedInMainFrame();
  void DidNavigateMainFrame();
  void FrontendLoaded();

  void JsonReceived(const DispatchCallback& callback,
                    int result,
                    const std::string& message);
  void DevicesDiscoveryConfigUpdated();
  void SendPortForwardingStatus(const base::Value& status);

  // DevToolsFileHelper::Delegate overrides.
  void FileSystemAdded(
      const DevToolsFileHelper::FileSystem& file_system) override;
  void FileSystemRemoved(const std::string& file_system_path) override;
  void FilePathsChanged(const std::vector<std::string>& changed_paths,
                        const std::vector<std::string>& added_paths,
                        const std::vector<std::string>& removed_paths) override;

  // DevToolsFileHelper callbacks.
  void FileSavedAs(const std::string& url);
  void CanceledFileSaveAs(const std::string& url);
  void AppendedTo(const std::string& url);
  void IndexingTotalWorkCalculated(int request_id,
                                   const std::string& file_system_path,
                                   int total_work);
  void IndexingWorked(int request_id,
                      const std::string& file_system_path,
                      int worked);
  void IndexingDone(int request_id, const std::string& file_system_path);
  void SearchCompleted(int request_id,
                       const std::string& file_system_path,
                       const std::vector<std::string>& file_paths);
  typedef base::Callback<void(bool)> InfoBarCallback;
  void ShowDevToolsConfirmInfoBar(const base::string16& message,
                                  const InfoBarCallback& callback);
  void UpdateFrontendHost();

  // Extensions support.
  void AddDevToolsExtensionsToClient();

  class FrontendWebContentsObserver;
  std::unique_ptr<FrontendWebContentsObserver> frontend_contents_observer_;

  Profile* profile_;
  DevToolsAndroidBridge* android_bridge_;
  content::WebContents* web_contents_;
  std::unique_ptr<Delegate> delegate_;
  scoped_refptr<content::DevToolsAgentHost> agent_host_;
  std::unique_ptr<content::DevToolsFrontendHost> frontend_host_;
  std::unique_ptr<DevToolsFileHelper> file_helper_;
  scoped_refptr<DevToolsFileSystemIndexer> file_system_indexer_;
  typedef std::map<
      int,
      scoped_refptr<DevToolsFileSystemIndexer::FileSystemIndexingJob> >
      IndexingJobsMap;
  IndexingJobsMap indexing_jobs_;

  bool devices_updates_enabled_;
  bool frontend_loaded_;
  bool reloading_;
  std::unique_ptr<DevToolsTargetsUIHandler> remote_targets_handler_;
  std::unique_ptr<PortForwardingStatusSerializer> port_status_serializer_;
  PrefChangeRegistrar pref_change_registrar_;
  std::unique_ptr<DevToolsEmbedderMessageDispatcher>
      embedder_message_dispatcher_;
  GURL url_;
  using PendingRequestsMap = std::map<const net::URLFetcher*, DispatchCallback>;
  PendingRequestsMap pending_requests_;
  base::WeakPtrFactory<DevToolsUIBindings> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsUIBindings);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_UI_BINDINGS_H_
