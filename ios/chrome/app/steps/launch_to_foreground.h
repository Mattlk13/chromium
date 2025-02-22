// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CHROME_APP_STEPS_LAUNCH_TO_FOREGROUND_H_
#define IOS_CHROME_APP_STEPS_LAUNCH_TO_FOREGROUND_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/app/application_step.h"

// This file includes ApplicationStep objects that perform the very first steps
// of application launch.

// Signals to the application context that the app is entering the foreground.
//  Pre:  Application phase is APPLICATION_BACKGROUNDED.
//  Post: Application phase is (still) APPLICATION_BACKGROUNDED.
@interface BeginForegrounding : NSObject<ApplicationStep>
@end

// Creates the main window and makes it key, but doesn't make it visible yet.
//  Pre:  Application phase is APPLICATION_BACKGROUNDED.
//  Post: Application phase is (still) APPLICATION_BACKGROUNDED.
@interface PrepareForUI : NSObject<ApplicationStep>
@end

// Performs final foregrounding tasks.
// Creates the main window and makes it key, but doesn't make it visible yet.
//  Pre:  Application phase is APPLICATION_BACKGROUNDED and the main window is
//        key.
//  Post: Application phase is APPLICATION_FOREGROUNDED.
@interface CompleteForegrounding : NSObject<ApplicationStep>
@end
#endif  // IOS_CHROME_APP_STEPS_LAUNCH_TO_FOREGROUND_H_
