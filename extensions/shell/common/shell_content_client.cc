// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/common/shell_content_client.h"

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/user_agent.h"
#include "extensions/common/constants.h"
#include "extensions/shell/common/version.h"  // Generated file.
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(DISABLE_NACL)
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "components/nacl/common/nacl_constants.h"
#include "components/nacl/renderer/plugin/ppapi_entrypoints.h"
#include "content/public/common/pepper_plugin_info.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#endif

namespace extensions {
namespace {

#if !defined(DISABLE_NACL)
bool GetNaClPluginPath(base::FilePath* path) {
  // On Posix, plugins live in the module directory.
  base::FilePath module;
  if (!PathService::Get(base::DIR_MODULE, &module))
    return false;
  *path = module.Append(nacl::kInternalNaClPluginFileName);
  return true;
}
#endif  // !defined(DISABLE_NACL)

}  // namespace

ShellContentClient::ShellContentClient() {
}

ShellContentClient::~ShellContentClient() {
}

void ShellContentClient::AddPepperPlugins(
    std::vector<content::PepperPluginInfo>* plugins) {
#if !defined(DISABLE_NACL)
  base::FilePath path;
  if (!GetNaClPluginPath(&path))
    return;

  content::PepperPluginInfo nacl;
  // The nacl plugin is now built into the binary.
  nacl.is_internal = true;
  nacl.path = path;
  nacl.name = nacl::kNaClPluginName;
  content::WebPluginMimeType nacl_mime_type(nacl::kNaClPluginMimeType,
                                            nacl::kNaClPluginExtension,
                                            nacl::kNaClPluginDescription);
  nacl.mime_types.push_back(nacl_mime_type);
  content::WebPluginMimeType pnacl_mime_type(nacl::kPnaclPluginMimeType,
                                             nacl::kPnaclPluginExtension,
                                             nacl::kPnaclPluginDescription);
  nacl.mime_types.push_back(pnacl_mime_type);
  nacl.internal_entry_points.get_interface = nacl_plugin::PPP_GetInterface;
  nacl.internal_entry_points.initialize_module =
      nacl_plugin::PPP_InitializeModule;
  nacl.internal_entry_points.shutdown_module =
      nacl_plugin::PPP_ShutdownModule;
  nacl.permissions = ppapi::PERMISSION_PRIVATE | ppapi::PERMISSION_DEV;
  plugins->push_back(nacl);
#endif  // !defined(DISABLE_NACL)
}

void ShellContentClient::AddAdditionalSchemes(
    std::vector<url::SchemeWithType>* standard_schemes,
    std::vector<url::SchemeWithType>* referrer_schemes,
    std::vector<std::string>* savable_schemes) {
  standard_schemes->push_back(url::SchemeWithType{extensions::kExtensionScheme,
                                                  url::SCHEME_WITHOUT_PORT});
  savable_schemes->push_back(kExtensionScheme);
}

std::string ShellContentClient::GetUserAgent() const {
  // Must contain a user agent string for version sniffing. For example,
  // pluginless WebRTC Hangouts checks the Chrome version number.
  return content::BuildUserAgentFromProduct("Chrome/" PRODUCT_VERSION);
}

base::string16 ShellContentClient::GetLocalizedString(int message_id) const {
  return l10n_util::GetStringUTF16(message_id);
}

base::StringPiece ShellContentClient::GetDataResource(
    int resource_id,
    ui::ScaleFactor scale_factor) const {
  return ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* ShellContentClient::GetDataResourceBytes(
    int resource_id) const {
  return ResourceBundle::GetSharedInstance().LoadDataResourceBytes(resource_id);
}

gfx::Image& ShellContentClient::GetNativeImageNamed(int resource_id) const {
  return ResourceBundle::GetSharedInstance().GetNativeImageNamed(resource_id);
}

}  // namespace extensions
