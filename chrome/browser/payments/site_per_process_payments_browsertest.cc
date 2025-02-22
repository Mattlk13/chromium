// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace payments {

class SitePerProcessPaymentsBrowserTest : public InProcessBrowserTest {
 public:
  SitePerProcessPaymentsBrowserTest() {}
  ~SitePerProcessPaymentsBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    // Append --site-per-process flag.
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    https_server_.reset(
        new net::EmbeddedTestServer(net::EmbeddedTestServer::TYPE_HTTPS));
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(https_server_->InitializeAndListen());
    content::SetupCrossSiteRedirector(https_server_.get());
    https_server_->ServeFilesFromSourceDirectory("chrome/test/data");
    https_server_->StartAcceptingConnections();
  }

  std::unique_ptr<net::EmbeddedTestServer> https_server_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SitePerProcessPaymentsBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SitePerProcessPaymentsBrowserTest,
                       IframePaymentRequestDoesNotCrash) {
  GURL url = https_server_->GetURL("a.com", "/payment_request_main.html");
  ui_test_utils::NavigateToURL(browser(), url);

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL iframe_url =
      https_server_->GetURL("b.com", "/payment_request_iframe.html");
  EXPECT_TRUE(content::NavigateIframeToURL(tab, "test", iframe_url));

  EXPECT_TRUE(tab->GetRenderWidgetHostView()->IsShowing());
  content::RenderFrameHost* frame = ChildFrameAt(tab->GetMainFrame(), 0);
  EXPECT_TRUE(frame);
  EXPECT_NE(frame->GetSiteInstance(), tab->GetMainFrame()->GetSiteInstance());
}

}  // namespace payments
