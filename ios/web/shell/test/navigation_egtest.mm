// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>
#import <XCTest/XCTest.h>

#import <EarlGrey/EarlGrey.h>

#include "base/strings/sys_string_conversions.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"
#include "ios/web/shell/test/app/web_view_interaction_test_util.h"
#import "ios/web/shell/test/earl_grey/shell_base_test_case.h"
#import "ios/web/shell/test/earl_grey/shell_earl_grey.h"
#import "ios/web/shell/test/earl_grey/shell_matchers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Navigation test cases for the web shell. These are Earl Grey integration
// tests, which are based on XCTest.
@interface NavigationTestCase : ShellBaseTestCase
@end

@implementation NavigationTestCase

// Tests clicking a link to about:blank.
- (void)testNavigationLinkToAboutBlank {
  const GURL URL = web::test::HttpServer::MakeUrl(
      "http://ios/web/shell/test/http_server_files/basic_navigation_test.html");
  web::test::SetUpFileBasedHttpServer();

  [ShellEarlGrey loadURL:URL];
  [[EarlGrey selectElementWithMatcher:web::addressFieldText(URL.spec())]
      assertWithMatcher:grey_notNil()];

  web::shell_test_util::TapWebViewElementWithId(
      "basic-link-navigation-to-about-blank");

  [[EarlGrey selectElementWithMatcher:web::addressFieldText("about:blank")]
      assertWithMatcher:grey_notNil()];
}

// TODO(crbug.com/675015): Re-enable this test on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testNavigationBackAndForward testNavigationBackAndForward
#else
#define MAYBE_testNavigationBackAndForward FLAKY_testNavigationBackAndForward
#endif
// Tests the back and forward button after entering two URLs.
- (void)MAYBE_testNavigationBackAndForward {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL1 = web::test::HttpServer::MakeUrl("http://firstURL");
  std::string response1 = "Test Page 1";
  responses[URL1] = response1;

  const GURL URL2 = web::test::HttpServer::MakeUrl("http://secondURL");
  std::string response2 = "Test Page 2";
  responses[URL2] = response2;

  web::test::SetUpSimpleHttpServer(responses);

  [ShellEarlGrey loadURL:URL1];
  [[EarlGrey selectElementWithMatcher:web::addressFieldText(URL1.spec())]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:web::webViewContainingText(response1)]
      assertWithMatcher:grey_notNil()];

  [ShellEarlGrey loadURL:URL2];
  [[EarlGrey selectElementWithMatcher:web::addressFieldText(URL2.spec())]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:web::webViewContainingText(response2)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:web::backButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:web::addressFieldText(URL1.spec())]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:web::webViewContainingText(response1)]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:web::forwardButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:web::addressFieldText(URL2.spec())]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:web::webViewContainingText(response2)]
      assertWithMatcher:grey_notNil()];
}

// TODO(crbug.com/675015): Re-enable this test on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testNavigationBackAndForwardAfterFragmentLink \
  testNavigationBackAndForwardAfterFragmentLink
#else
#define MAYBE_testNavigationBackAndForwardAfterFragmentLink \
  FLAKY_testNavigationBackAndForwardAfterFragmentLink
#endif
// Tests back and forward navigation where a fragment link is tapped.
- (void)MAYBE_testNavigationBackAndForwardAfterFragmentLink {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL1 = web::test::HttpServer::MakeUrl("http://fragmentLink");
  const std::string response = "<a href='#hash' id='link'>link</a>";
  responses[URL1] = response;

  const GURL URL2 = web::test::HttpServer::MakeUrl("http://fragmentLink/#hash");

  web::test::SetUpSimpleHttpServer(responses);

  [ShellEarlGrey loadURL:URL1];
  [[EarlGrey selectElementWithMatcher:web::addressFieldText(URL1.spec())]
      assertWithMatcher:grey_notNil()];

  web::shell_test_util::TapWebViewElementWithId("link");
  [[EarlGrey selectElementWithMatcher:web::addressFieldText(URL2.spec())]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:web::backButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:web::addressFieldText(URL1.spec())]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:web::forwardButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:web::addressFieldText(URL2.spec())]
      assertWithMatcher:grey_notNil()];
}

// Tests tapping a link with onclick="event.preventDefault()" and verifies that
// the URL didn't change..
- (void)testNavigationLinkPreventDefaultOverridesHref {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL = web::test::HttpServer::MakeUrl("http://overridesHrefLink");
  const char pageHTML[] =
      "<script>"
      "  function printMsg() {"
      "    document.body.appendChild(document.createTextNode('Default "
      "prevented!'));"
      "  }"
      "</script>"
      "<a href='#hash' id='overrides-href' onclick='event.preventDefault(); "
      "printMsg();'>redirectLink</a>";
  responses[URL] = pageHTML;

  web::test::SetUpSimpleHttpServer(responses);

  [ShellEarlGrey loadURL:URL];
  [[EarlGrey selectElementWithMatcher:web::addressFieldText(URL.spec())]
      assertWithMatcher:grey_notNil()];

  web::shell_test_util::TapWebViewElementWithId("overrides-href");

  [[EarlGrey selectElementWithMatcher:web::addressFieldText(URL.spec())]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:web::webViewContainingText("Default prevented!")]
      assertWithMatcher:grey_notNil()];
}

// Tests tapping on a link with unsupported URL scheme.
- (void)testNavigationUnsupportedSchema {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  const GURL URL =
      web::test::HttpServer::MakeUrl("http://urlWithUnsupportedSchemeLink");
  const char pageHTML[] =
      "<script>"
      "  function printMsg() {"
      "    document.body.appendChild(document.createTextNode("
      "    'No navigation!'));"
      "  }"
      "</script>"
      "<a href='aaa://unsupported' id='link' "
      "onclick='printMsg();'>unsupportedScheme</a>";
  responses[URL] = pageHTML;

  web::test::SetUpSimpleHttpServer(responses);

  [ShellEarlGrey loadURL:URL];
  [[EarlGrey selectElementWithMatcher:web::addressFieldText(URL.spec())]
      assertWithMatcher:grey_notNil()];

  web::shell_test_util::TapWebViewElementWithId("link");

  [[EarlGrey selectElementWithMatcher:web::addressFieldText(URL.spec())]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey
      selectElementWithMatcher:web::webViewContainingText("No navigation!")]
      assertWithMatcher:grey_notNil()];
}

@end
