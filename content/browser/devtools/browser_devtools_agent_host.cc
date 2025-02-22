// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/browser_devtools_agent_host.h"

#include "base/bind.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/io_handler.h"
#include "content/browser/devtools/protocol/memory_handler.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/devtools/protocol/system_info_handler.h"
#include "content/browser/devtools/protocol/tethering_handler.h"
#include "content/browser/devtools/protocol/tracing_handler.h"
#include "content/browser/frame_host/frame_tree_node.h"

namespace content {

scoped_refptr<DevToolsAgentHost> DevToolsAgentHost::CreateForBrowser(
    scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner,
    const CreateServerSocketCallback& socket_callback) {
  return new BrowserDevToolsAgentHost(tethering_task_runner, socket_callback);
}

BrowserDevToolsAgentHost::BrowserDevToolsAgentHost(
    scoped_refptr<base::SingleThreadTaskRunner> tethering_task_runner,
    const CreateServerSocketCallback& socket_callback)
    : DevToolsAgentHostImpl(base::GenerateGUID()),
      tethering_task_runner_(tethering_task_runner),
      socket_callback_(socket_callback) {
  NotifyCreated();
}

BrowserDevToolsAgentHost::~BrowserDevToolsAgentHost() {
}

void BrowserDevToolsAgentHost::AttachSession(DevToolsSession* session) {
  session->AddHandler(base::WrapUnique(new protocol::IOHandler(
      GetIOContext())));
  session->AddHandler(base::WrapUnique(new protocol::MemoryHandler()));
  session->AddHandler(base::WrapUnique(new protocol::SystemInfoHandler()));
  session->AddHandler(base::WrapUnique(new protocol::TetheringHandler(
      socket_callback_, tethering_task_runner_)));
  session->AddHandler(base::WrapUnique(new protocol::TracingHandler(
      protocol::TracingHandler::Browser,
      FrameTreeNode::kFrameTreeNodeInvalidId,
      GetIOContext())));
}

void BrowserDevToolsAgentHost::DetachSession(int session_id) {
}

std::string BrowserDevToolsAgentHost::GetType() {
  return kTypeBrowser;
}

std::string BrowserDevToolsAgentHost::GetTitle() {
  return "";
}

GURL BrowserDevToolsAgentHost::GetURL() {
  return GURL();
}

bool BrowserDevToolsAgentHost::Activate() {
  return false;
}

bool BrowserDevToolsAgentHost::Close() {
  return false;
}

void BrowserDevToolsAgentHost::Reload() {
}

bool BrowserDevToolsAgentHost::DispatchProtocolMessage(
    DevToolsSession* session,
    const std::string& message) {
  int call_id;
  std::string method;
  session->Dispatch(message, false, &call_id, &method);
  return true;
}

}  // content
