// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/supervised_user_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/supervised_user_internals_message_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace {

content::WebUIDataSource* CreateSupervisedUserInternalsHTMLSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      chrome::kChromeUISupervisedUserInternalsHost);

  source->AddResourcePath("supervised_user_internals.js",
                          IDR_SUPERVISED_USER_INTERNALS_JS);
  source->AddResourcePath("supervised_user_internals.css",
                          IDR_SUPERVISED_USER_INTERNALS_CSS);
  source->SetDefaultResource(IDR_SUPERVISED_USER_INTERNALS_HTML);
  source->UseGzip(std::unordered_set<std::string>());
  return source;
}

}  // namespace

SupervisedUserInternalsUI::SupervisedUserInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile,
                                CreateSupervisedUserInternalsHTMLSource());

  web_ui->AddMessageHandler(new SupervisedUserInternalsMessageHandler);
}

SupervisedUserInternalsUI::~SupervisedUserInternalsUI() {}
