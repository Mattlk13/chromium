// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the constants used to process master_preferences files
// used by setup and first run.

#ifndef CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_CONSTANTS_H_
#define CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_CONSTANTS_H_

namespace installer {
namespace master_preferences {
// All the preferences below are expected to be inside the JSON "distribution"
// block. Some of them also have equivalent command line option. If same option
// is specified in master preference as well as command line, the command line
// value takes precedence.

// Boolean. This is a legacy preference and should no longer be used; it is
// kept around so that old master_preferences which specify
// "create_all_shortcuts":false still enforce the new
// "do_not_create_(desktop|quick_launch)_shortcut" preferences. Setting this to
// true no longer has any impact.
extern const char kCreateAllShortcuts[];
// Boolean pref that disables all logging.
extern const char kDisableLogging[];
// Name of the dictionary that holds the distribution values.
extern const char kDistroDict[];
// Boolean pref that triggers silent import of the default browser bookmarks.
extern const char kDistroImportBookmarksPref[];
// String pref that triggers silent import of bookmarks from the html file at
// given path.
extern const char kDistroImportBookmarksFromFilePref[];
// Boolean pref that triggers silent import of the default browser history.
extern const char kDistroImportHistoryPref[];
// Boolean pref that triggers silent import of the default browser homepage.
extern const char kDistroImportHomePagePref[];
// Boolean pref that triggers silent import of the default search engine.
extern const char kDistroImportSearchPref[];
// Integer. RLZ ping delay in seconds.
extern const char kDistroPingDelay[];
// String of Chrome version for which the "set as default browser" infobar will
// never be shown.
extern const char kDistroSuppressDefaultBrowserPromptPref[];
// Boolean. Do not show first run bubble, even if it would otherwise be shown.
extern const char kDistroSuppressFirstRunBubble[];
// Boolean that specifies whether or not showing the welcome page following an
// OS upgrade is enabled. True by default. May be overridden by the
// WelcomePageOnOSUpgradeEnabled policy setting.
extern const char kDistroWelcomePageOnOSUpgradeEnabled[];
// Boolean. Prevent creation of all shortcuts to chrome, including the
// desktop, quick launch, taskbar and the start menu shortcuts.
extern const char kDoNotCreateAnyShortcuts[];
// Boolean. Prevent creation of the Desktop shortcut on install (and later on
// Active Setup for each user on a system-level install).
extern const char kDoNotCreateDesktopShortcut[];
// Boolean. Prevent creation of the Quick Launch shortcut on install (and later
// on Active Setup for each user on a system-level install).
extern const char kDoNotCreateQuickLaunchShortcut[];
// Boolean. Prevent creation of the Taskbar (since Windows 7) shortcut on
// install (and later on Active Setup for each user on a system-level install).
extern const char kDoNotCreateTaskbarShortcut[];
// Boolean. Do not launch Chrome after first install. Cmd line override present.
extern const char kDoNotLaunchChrome[];
// Boolean. Do not register with Google Update to have Chrome launched after
// install. Cmd line override present.
extern const char kDoNotRegisterForUpdateLaunch[];
// String.  Specifies the file path to write logging info to.
extern const char kLogFile[];
// Boolean. Register Chrome as default browser. Cmd line override present.
extern const char kMakeChromeDefault[];
// Boolean. Register Chrome as default browser for the current user.
extern const char kMakeChromeDefaultForUser[];
// Boolean. Expect to be run by an MSI installer. Cmd line override present.
extern const char kMsi[];
// String. The MSI Product ID under which the MSI stores its information.  This
// is used to update the DisplayVersion to match Chrome's version number.
extern const char kMsiProductId[];
// Boolean. Support installing multiple products at once.
extern const char kMultiInstall[];
// Boolean. Show EULA dialog before install.
extern const char kRequireEula[];
// Boolean. Indicates that the first-run 'set-as-default' dialog should not be
// shown. Relevant in Windows 8+ context only. If this is true, the standard
// 'set default browser' prompt on the butter-bar will appear during the first
// run.
extern const char kSuppressFirstRunDefaultBrowserPrompt[];
// Boolean. Install Chrome to system wise location. Cmd line override present.
extern const char kSystemLevel[];
// Boolean. Run installer in verbose mode. Cmd line override present.
extern const char kVerboseLogging[];
// Name of the block that contains the extensions on the master preferences.
extern const char kExtensionsBlock[];
// Boolean. Allow Chrome to be downgraded to a previous version if true.
extern const char kAllowDowngrade[];

}  // namespace master_preferences
}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_MASTER_PREFERENCES_CONSTANTS_H_
