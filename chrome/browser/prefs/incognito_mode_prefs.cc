// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/incognito_mode_prefs.h"

#include <stdint.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#include <windows.h>
#include <wpcapi.h>
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/singleton.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#endif  // OS_WIN

#if BUILDFLAG(ANDROID_JAVA_UI)
#include "chrome/browser/android/chrome_application.h"
#endif  // BUILDFLAG(ANDROID_JAVA_UI)

using content::BrowserThread;

#if defined(OS_WIN)
namespace {

// This singleton allows us to attempt to calculate the Platform Parental
// Controls enabled value on a worker thread before the UI thread needs the
// value. If the UI thread finishes sooner than we expect, that's no worse than
// today where we block.
class PlatformParentalControlsValue {
 public:
  static PlatformParentalControlsValue* GetInstance() {
    return base::Singleton<PlatformParentalControlsValue>::get();
  }

  bool is_enabled() const {
    return is_enabled_;
  }

 private:
  friend struct base::DefaultSingletonTraits<PlatformParentalControlsValue>;

  // Histogram enum for tracking the thread that checked parental controls.
  enum class ThreadType {
    UI = 0,
    BLOCKING,
    COUNT,
  };

  PlatformParentalControlsValue()
      : is_enabled_(IsParentalControlActivityLoggingOn()) {}

  ~PlatformParentalControlsValue() = default;

  // Returns true if Windows Parental control activity logging is enabled. This
  // feature is available on Windows 7 and beyond. This function should be
  // called on a COM Initialized thread and is potentially blocking.
  static bool IsParentalControlActivityLoggingOn() {
    // Since we can potentially block, make sure the thread is okay with this.
    base::ThreadRestrictions::AssertIOAllowed();

    // Query this info on Windows 7 and above.
    if (base::win::GetVersion() < base::win::VERSION_WIN7)
      return false;

    ThreadType thread_type = ThreadType::BLOCKING;
    if (BrowserThread::IsThreadInitialized(BrowserThread::UI) &&
        content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
      thread_type = ThreadType::UI;
    }

    UMA_HISTOGRAM_ENUMERATION(
        "IncognitoModePrefs.WindowsParentalControlsInitThread",
        static_cast<int32_t>(thread_type),
        static_cast<int32_t>(ThreadType::COUNT));

    base::Time begin_time = base::Time::Now();
    bool result = IsParentalControlActivityLoggingOnImpl();
    UMA_HISTOGRAM_TIMES("IncognitoModePrefs.WindowsParentalControlsInitTime",
                        base::Time::Now() - begin_time);
    return result;
  }

  // Does the work of determining if Windows Parental control activity logging
  // is enabled.
  static bool IsParentalControlActivityLoggingOnImpl() {
    base::win::ScopedComPtr<IWindowsParentalControlsCore> parent_controls;
    HRESULT hr = parent_controls.CreateInstance(
        __uuidof(WindowsParentalControls));
    if (FAILED(hr))
      return false;

    base::win::ScopedComPtr<IWPCSettings> settings;
    hr = parent_controls->GetUserSettings(nullptr, settings.Receive());
    if (FAILED(hr))
      return false;

    unsigned long restrictions = 0;
    settings->GetRestrictions(&restrictions);

    return (restrictions & WPCFLAG_LOGGING_REQUIRED) ==
        WPCFLAG_LOGGING_REQUIRED;
  }

  const bool is_enabled_;

  DISALLOW_COPY_AND_ASSIGN(PlatformParentalControlsValue);
};

}  // namespace
#endif  // OS_WIN

// static
bool IncognitoModePrefs::IntToAvailability(int in_value,
                                           Availability* out_value) {
  if (in_value < 0 || in_value >= AVAILABILITY_NUM_TYPES) {
    *out_value = ENABLED;
    return false;
  }
  *out_value = static_cast<Availability>(in_value);
  return true;
}

// static
IncognitoModePrefs::Availability IncognitoModePrefs::GetAvailability(
    const PrefService* pref_service) {
  return GetAvailabilityInternal(pref_service, CHECK_PARENTAL_CONTROLS);
}

// static
void IncognitoModePrefs::SetAvailability(PrefService* prefs,
                                         const Availability availability) {
  prefs->SetInteger(prefs::kIncognitoModeAvailability, availability);
}

// static
void IncognitoModePrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(prefs::kIncognitoModeAvailability,
                                IncognitoModePrefs::ENABLED);
}

// static
bool IncognitoModePrefs::ShouldLaunchIncognito(
    const base::CommandLine& command_line,
    const PrefService* prefs) {
  // Note: This code only checks parental controls if the user requested
  // to launch in incognito mode or if it was forced via prefs. This way,
  // the parental controls check (which can be quite slow) can be avoided
  // most of the time.
  const bool should_use_incognito =
      command_line.HasSwitch(switches::kIncognito) ||
      GetAvailabilityInternal(prefs, DONT_CHECK_PARENTAL_CONTROLS) ==
          IncognitoModePrefs::FORCED;
  return should_use_incognito &&
         GetAvailabilityInternal(prefs, CHECK_PARENTAL_CONTROLS) !=
             IncognitoModePrefs::DISABLED;
}

// static
bool IncognitoModePrefs::CanOpenBrowser(Profile* profile) {
  if (profile->IsGuestSession())
    return true;

  switch (GetAvailability(profile->GetPrefs())) {
    case IncognitoModePrefs::ENABLED:
      return true;

    case IncognitoModePrefs::DISABLED:
      return !profile->IsOffTheRecord();

    case IncognitoModePrefs::FORCED:
      return profile->IsOffTheRecord();

    default:
      NOTREACHED();
      return false;
  }
}

#if defined(OS_WIN)
// static
void IncognitoModePrefs::InitializePlatformParentalControls() {
  content::BrowserThread::PostBlockingPoolTask(
      FROM_HERE,
      base::Bind(
          base::IgnoreResult(&PlatformParentalControlsValue::GetInstance)));
}
#endif

// static
bool IncognitoModePrefs::ArePlatformParentalControlsEnabled() {
#if defined(OS_WIN)
  return PlatformParentalControlsValue::GetInstance()->is_enabled();
#elif BUILDFLAG(ANDROID_JAVA_UI)
  return chrome::android::ChromeApplication::AreParentalControlsEnabled();
#else
  return false;
#endif
}

// static
IncognitoModePrefs::Availability IncognitoModePrefs::GetAvailabilityInternal(
    const PrefService* pref_service,
    GetAvailabilityMode mode) {
  DCHECK(pref_service);
  int pref_value = pref_service->GetInteger(prefs::kIncognitoModeAvailability);
  Availability result = IncognitoModePrefs::ENABLED;
  bool valid = IntToAvailability(pref_value, &result);
  DCHECK(valid);
  if (result != IncognitoModePrefs::DISABLED &&
      mode == CHECK_PARENTAL_CONTROLS && ArePlatformParentalControlsEnabled()) {
    if (result == IncognitoModePrefs::FORCED)
      LOG(ERROR) << "Ignoring FORCED incognito. Parental control logging on";
    return IncognitoModePrefs::DISABLED;
  }
  return result;
}
