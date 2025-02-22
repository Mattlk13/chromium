// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/settings/settings_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SettingsCoordinator

@synthesize baseViewController;

- (void)start {
  UIViewController* settings = [[UIViewController alloc] init];
  settings.title = @"Settings";
  settings.view.backgroundColor = [UIColor grayColor];
  [self.baseViewController pushViewController:settings animated:YES];
}

@end
