// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_INTERSTITIAL_PAGE_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_INTERSTITIAL_PAGE_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace content {
class InterstitialPage;
class WebContents;
}

namespace security_interstitials {
class MetricsHelper;
class SecurityInterstitialControllerClient;

class SecurityInterstitialPage : public content::InterstitialPageDelegate {
 public:
  SecurityInterstitialPage(
      content::WebContents* web_contents,
      const GURL& url,
      std::unique_ptr<SecurityInterstitialControllerClient> controller);
  ~SecurityInterstitialPage() override;

  // Creates an interstitial and shows it.
  virtual void Show();

  // Prevents creating the actual interstitial view for testing.
  void DontCreateViewForTesting();

 protected:
  // Returns true if the interstitial should create a new navigation entry.
  virtual bool ShouldCreateNewNavigation() const = 0;

  // Populates the strings used to generate the HTML from the template.
  virtual void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) = 0;

  // Gives an opportunity for child classes to react to Show() having run. The
  // interstitial_page_ will now have a value.
  virtual void AfterShow() {}

  // InterstitialPageDelegate method:
  std::string GetHTMLContents() override;

  // Returns the formatted host name for the request url.
  base::string16 GetFormattedHostName() const;

  content::InterstitialPage* interstitial_page() const;
  content::WebContents* web_contents() const;
  GURL request_url() const;

  // Returns the boolean value of the given |pref|.
  bool IsPrefEnabled(const char* pref);

  SecurityInterstitialControllerClient* controller();

  MetricsHelper* metrics_helper();

 private:
  // The WebContents with which this interstitial page is
  // associated. Not available in ~SecurityInterstitialPage, since it
  // can be destroyed before this class is destroyed.
  content::WebContents* web_contents_;
  const GURL request_url_;
  // Once shown, |interstitial_page| takes ownership of this
  // SecurityInterstitialPage instance.
  content::InterstitialPage* interstitial_page_;
  // Whether the interstitial should create a view.
  bool create_view_;
  // For subclasses that don't have their own ControllerClients yet.
  std::unique_ptr<SecurityInterstitialControllerClient> controller_;

  std::unique_ptr<MetricsHelper> metrics_helper_;

  DISALLOW_COPY_AND_ASSIGN(SecurityInterstitialPage);
};

}  // security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_SECURITY_INTERSTITIAL_PAGE_H_
