// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/url_opener.h"

#import <Foundation/Foundation.h>

#import "base/ios/weak_nsobject.h"
#import "base/mac/scoped_nsobject.h"
#include "base/metrics/histogram_macros.h"
#import "ios/chrome/app/application_delegate/app_state.h"
#import "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#include "ios/chrome/app/chrome_app_startup_parameters.h"

namespace {
// Key of the UMA Startup.MobileSessionStartFromApps histogram.
const char* const kUMAMobileSessionStartFromAppsHistogram =
    "Startup.MobileSessionStartFromApps";
}  // namespace

@implementation URLOpener

- (instancetype)init {
  NOTREACHED();
  return nil;
}

#pragma mark - ApplicationDelegate - URL Opening methods

+ (BOOL)openURL:(NSURL*)url
     applicationActive:(BOOL)applicationActive
               options:(NSDictionary<NSString*, id>*)options
             tabOpener:(id<TabOpening>)tabOpener
    startupInformation:(id<StartupInformation>)startupInformation {
  NSString* sourceApplication =
      options[UIApplicationOpenURLOptionsSourceApplicationKey];
  base::scoped_nsobject<ChromeAppStartupParameters> params(
      [ChromeAppStartupParameters
          newChromeAppStartupParametersWithURL:url
                         fromSourceApplication:sourceApplication]);

  MobileSessionCallerApp callerApp = [params callerApp];

  UMA_HISTOGRAM_ENUMERATION(kUMAMobileSessionStartFromAppsHistogram, callerApp,
                            MOBILE_SESSION_CALLER_APP_COUNT);

  if (startupInformation.isPresentingFirstRunUI) {
    UMA_HISTOGRAM_ENUMERATION("FirstRun.LaunchSource", [params launchSource],
                              first_run::LAUNCH_SIZE);
  }

  if (applicationActive) {
    // The app is already active so the applicationDidBecomeActive: method will
    // never be called. Open the requested URL immediately and return YES if
    // the parsed URL was valid.
    if (params) {
      [tabOpener dismissModalsAndOpenSelectedTabInMode:ApplicationMode::NORMAL
                                               withURL:[params externalURL]
                                            transition:ui::PAGE_TRANSITION_LINK
                                            completion:nil];
      return YES;
    }
    return NO;
  }

  // Don't record the first user action.
  [startupInformation resetFirstUserActionRecorder];

  startupInformation.startupParameters = params;
  return startupInformation.startupParameters != nil;
}

+ (void)handleLaunchOptions:(NSDictionary*)launchOptions
          applicationActive:(BOOL)applicationActive
                  tabOpener:(id<TabOpening>)tabOpener
         startupInformation:(id<StartupInformation>)startupInformation
                   appState:(AppState*)appState {
  NSURL* url = launchOptions[UIApplicationLaunchOptionsURLKey];
  NSString* sourceApplication =
      launchOptions[UIApplicationLaunchOptionsSourceApplicationKey];

  if (url && sourceApplication) {
    NSDictionary<NSString*, id>* options =
        @{UIApplicationOpenURLOptionsSourceApplicationKey : sourceApplication};

    BOOL openURLResult = [URLOpener openURL:url
                          applicationActive:applicationActive
                                    options:options
                                  tabOpener:tabOpener
                         startupInformation:startupInformation];
    [appState launchFromURLHandled:openURLResult];
  }
}

@end
