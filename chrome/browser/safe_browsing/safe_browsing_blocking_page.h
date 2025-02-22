// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Classes for managing the SafeBrowsing interstitial pages.
//
// When a user is about to visit a page the SafeBrowsing system has deemed to
// be malicious, either as malware or a phishing page, we show an interstitial
// page with some options (go back, continue) to give the user a chance to avoid
// the harmful page.
//
// The SafeBrowsingBlockingPage is created by the SafeBrowsingUIManager on the
// UI thread when we've determined that a page is malicious. The operation of
// the blocking page occurs on the UI thread, where it waits for the user to
// make a decision about what to do: either go back or continue on.
//
// The blocking page forwards the result of the user's choice back to the
// SafeBrowsingUIManager so that we can cancel the request for the new page,
// or allow it to continue.
//
// A web page may contain several resources flagged as malware/phishing.  This
// results into more than one interstitial being shown.  On the first unsafe
// resource received we show an interstitial.  Any subsequent unsafe resource
// notifications while the first interstitial is showing is queued.  If the user
// decides to proceed in the first interstitial, we display all queued unsafe
// resources in a new interstitial.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_

#include <map>
#include <string>
#include <vector>

#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/interstitials/chrome_metrics_helper.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "components/security_interstitials/content/security_interstitial_page.h"
#include "components/security_interstitials/core/safe_browsing_error_ui.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "url/gurl.h"

namespace safe_browsing {

class SafeBrowsingBlockingPageFactory;
class ThreatDetails;

class SafeBrowsingBlockingPage
    : public security_interstitials::SecurityInterstitialPage {
 public:
  typedef security_interstitials::UnsafeResource UnsafeResource;
  typedef security_interstitials::SafeBrowsingErrorUI SafeBrowsingErrorUI;
  typedef std::vector<UnsafeResource> UnsafeResourceList;
  typedef std::map<content::WebContents*, UnsafeResourceList> UnsafeResourceMap;

  // Interstitial type, used in tests.
  static content::InterstitialPageDelegate::TypeID kTypeForTesting;

  ~SafeBrowsingBlockingPage() override;

  // Creates a blocking page. Use ShowBlockingPage if you don't need to access
  // the blocking page directly.
  static SafeBrowsingBlockingPage* CreateBlockingPage(
      SafeBrowsingUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const UnsafeResource& unsafe_resource);

  // Shows a blocking page warning the user about phishing/malware for a
  // specific resource.
  // You can call this method several times, if an interstitial is already
  // showing, the new one will be queued and displayed if the user decides
  // to proceed on the currently showing interstitial.
  static void ShowBlockingPage(
      SafeBrowsingUIManager* ui_manager, const UnsafeResource& resource);

  // Makes the passed |factory| the factory used to instantiate
  // SafeBrowsingBlockingPage objects. Useful for tests.
  static void RegisterFactory(SafeBrowsingBlockingPageFactory* factory) {
    factory_ = factory;
  }

  // InterstitialPageDelegate method:
  void OnProceed() override;
  void OnDontProceed() override;
  void CommandReceived(const std::string& command) override;
  void OverrideRendererPrefs(content::RendererPreferences* prefs) override;
  content::InterstitialPageDelegate::TypeID GetTypeForTesting() const override;

  // Checks the threat type to decide if we should report ThreatDetails.
  static bool ShouldReportThreatDetails(SBThreatType threat_type);

 protected:
  friend class SafeBrowsingBlockingPageFactoryImpl;
  friend class SafeBrowsingBlockingPageTest;
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           ProceedThenDontProceed);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           MalwareReportsDisabled);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           MalwareReportsToggling);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           ExtendedReportingNotShownOnSecurePage);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           MalwareReportsTransitionDisabled);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           ExtendedReportingNotShownInIncognito);
  FRIEND_TEST_ALL_PREFIXES(SafeBrowsingBlockingPageTest,
                           ExtendedReportingNotShownNotAllowExtendedReporting);

  void UpdateReportingPref();  // Used for the transition from old to new pref.

  // Don't instantiate this class directly, use ShowBlockingPage instead.
  SafeBrowsingBlockingPage(SafeBrowsingUIManager* ui_manager,
                           content::WebContents* web_contents,
                           const GURL& main_frame_url,
                           const UnsafeResourceList& unsafe_resources);

  // SecurityInterstitialPage methods:
  bool ShouldCreateNewNavigation() const override;
  void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) override;

  // After a safe browsing interstitial where the user opted-in to the
  // report but clicked "proceed anyway", we delay the call to
  // ThreatDetails::FinishCollection() by this much time (in
  // milliseconds), in order to get data from the blocked resource itself.
  int64_t threat_details_proceed_delay_ms_;

  // Called when the interstitial is going away. If there is a
  // pending threat details object, we look at the user's
  // preferences, and if the option to send threat details is
  // enabled, the report is scheduled to be sent on the |ui_manager_|.
  void FinishThreatDetails(int64_t delay_ms, bool did_proceed, int num_visits);

  // A list of SafeBrowsingUIManager::UnsafeResource for a tab that the user
  // should be warned about.  They are queued when displaying more than one
  // interstitial at a time.
  static UnsafeResourceMap* GetUnsafeResourcesMap();

  // Returns true if the passed |unsafe_resources| is blocking the load of
  // the main page.
  static bool IsMainPageLoadBlocked(
      const UnsafeResourceList& unsafe_resources);

  // For reporting back user actions.
  SafeBrowsingUIManager* ui_manager_;

  // For displaying safe browsing interstitial.
  std::unique_ptr<SafeBrowsingErrorUI> sb_error_ui_;

  // The URL of the main frame that caused the warning.
  GURL main_frame_url_;

  // The index of a navigation entry that should be removed when DontProceed()
  // is invoked, -1 if not entry should be removed.
  int navigation_entry_index_to_remove_;

  // The list of unsafe resources this page is warning about.
  UnsafeResourceList unsafe_resources_;

  // A ThreatDetails object that we start generating when the
  // blocking page is shown. The object will be sent when the warning
  // is gone (if the user enables the feature).
  scoped_refptr<ThreatDetails> threat_details_;

  bool proceeded_;

  // Which type of Safe Browsing interstitial this is.
  SafeBrowsingErrorUI::SBInterstitialReason interstitial_reason_;

  // The factory used to instantiate SafeBrowsingBlockingPage objects.
  // Useful for tests, so they can provide their own implementation of
  // SafeBrowsingBlockingPage.
  static SafeBrowsingBlockingPageFactory* factory_;

 private:
  static std::string GetMetricPrefix(
      const UnsafeResourceList& unsafe_resources,
      SafeBrowsingErrorUI::SBInterstitialReason interstitial_reason);
  static std::string GetExtraMetricsSuffix(
      const UnsafeResourceList& unsafe_resources);
  static std::string GetSamplingEventName(
      SafeBrowsingErrorUI::SBInterstitialReason interstitial_reason);

  static SafeBrowsingErrorUI::SBInterstitialReason GetInterstitialReason(
      const UnsafeResourceList& unsafe_resources);

  std::unique_ptr<security_interstitials::SecurityInterstitialControllerClient>
  CreateControllerClient(
      content::WebContents* web_contents,
      const UnsafeResourceList& unsafe_resources);

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingBlockingPage);
};

// Factory for creating SafeBrowsingBlockingPage.  Useful for tests.
class SafeBrowsingBlockingPageFactory {
 public:
  virtual ~SafeBrowsingBlockingPageFactory() { }

  virtual SafeBrowsingBlockingPage* CreateSafeBrowsingPage(
      SafeBrowsingUIManager* ui_manager,
      content::WebContents* web_contents,
      const GURL& main_frame_url,
      const SafeBrowsingBlockingPage::UnsafeResourceList& unsafe_resources) = 0;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_BLOCKING_PAGE_H_
