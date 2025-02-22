// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_SETTINGS_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_SETTINGS_COMMANDS_H_

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

// Command protocol for commands relating to the Settings UI.
// (Commands are for communicating into or within the coordinator layer).
@protocol SettingsCommands
// Display the settings UI.
- (void)showSettings;
// Dismiss the settings UI.
- (void)closeSettings;
@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_SETTINGS_COMMANDS_H_
