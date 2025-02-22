// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_properties.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/themes/browser_theme_pack.h"
#include "chrome/grit/theme_resources.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/gfx/color_palette.h"
#include "ui/resources/grit/ui_resources.h"

namespace {

// ----------------------------------------------------------------------------
// Defaults for properties which are stored in the browser theme pack. If you
// change these defaults, you must increment the version number in
// browser_theme_pack.cc.

const SkColor kDefaultColorFrame = SkColorSetRGB(0xCC, 0xCC, 0xCC);

#if defined(OS_MACOSX)
const SkColor kDefaultColorFrameIncognito =
    SkColorSetARGB(0xE6, 0x14, 0x16, 0x18);
const SkColor kDefaultColorFrameIncognitoInactiveMac =
    SkColorSetRGB(0x1E, 0x1E, 0x1E);
#else
const SkColor kDefaultColorFrameIncognito = SkColorSetRGB(0x28, 0x2B, 0x2D);
#endif

const SkColor kDefaultColorToolbar = SkColorSetRGB(0xF2, 0xF2, 0xF2);
const SkColor kDefaultColorToolbarIncognito = SkColorSetRGB(0x50, 0x50, 0x50);

const SkColor kDefaultDetachedBookmarkBarBackground = SK_ColorWHITE;
const SkColor kDefaultDetachedBookmarkBarBackgroundIncognito =
    SkColorSetRGB(0x32, 0x32, 0x32);

// "Toolbar" text is used for active tabs and the bookmarks bar.
constexpr SkColor kDefaultColorToolbarText = SK_ColorBLACK;
constexpr SkColor kDefaultColorToolbarTextIncognito = SK_ColorWHITE;
constexpr SkColor kDefaultColorBackgroundTabText = SK_ColorBLACK;
constexpr SkColor kDefaultColorBackgroundTabTextIncognito = SK_ColorWHITE;

const SkColor kDefaultColorBookmarkInstructionsText =
    SkColorSetRGB(0x64, 0x64, 0x64);
const SkColor kDefaultColorBookmarkInstructionsTextIncognito =
    SkColorSetA(SK_ColorWHITE, 0x8A);

#if defined(OS_WIN)
const SkColor kDefaultColorNTPBackground =
    color_utils::GetSysSkColor(COLOR_WINDOW);
const SkColor kDefaultColorNTPText =
    color_utils::GetSysSkColor(COLOR_WINDOWTEXT);
const SkColor kDefaultColorNTPLink = color_utils::GetSysSkColor(COLOR_HOTLIGHT);
#else
// TODO(beng): source from theme provider.
constexpr SkColor kDefaultColorNTPBackground = SK_ColorWHITE;
constexpr SkColor kDefaultColorNTPText = SK_ColorBLACK;
const SkColor kDefaultColorNTPLink = SkColorSetRGB(0x06, 0x37, 0x74);
#endif  // OS_WIN

const SkColor kDefaultColorNTPHeader = SkColorSetRGB(0x96, 0x96, 0x96);
const SkColor kDefaultColorNTPSection = SkColorSetRGB(0xE5, 0xE5, 0xE5);
constexpr SkColor kDefaultColorNTPSectionText = SK_ColorBLACK;
const SkColor kDefaultColorNTPSectionLink = SkColorSetRGB(0x06, 0x37, 0x74);
constexpr SkColor kDefaultColorButtonBackground = SK_ColorTRANSPARENT;

// Default tints.
constexpr color_utils::HSL kDefaultTintButtons = {-1, -1, -1};
constexpr color_utils::HSL kDefaultTintButtonsIncognito = {-1, -1, 0.85};
constexpr color_utils::HSL kDefaultTintFrame = {-1, -1, -1};
constexpr color_utils::HSL kDefaultTintFrameInactive = {-1, -1, 0.9};
constexpr color_utils::HSL kDefaultTintFrameIncognito = {-1, 0.2, 0.35};
constexpr color_utils::HSL kDefaultTintFrameIncognitoInactive = {-1, 0.2, 0.87};
constexpr color_utils::HSL kDefaultTintBackgroundTab = {-1, -1, 0.75};

// ----------------------------------------------------------------------------
// Defaults for properties which are not stored in the browser theme pack.

constexpr SkColor kDefaultColorControlBackground = SK_ColorWHITE;
const SkColor kDefaultDetachedBookmarkBarSeparator =
    SkColorSetRGB(0xB6, 0xB4, 0xB6);
const SkColor kDefaultDetachedBookmarkBarSeparatorIncognito =
    SkColorSetRGB(0x28, 0x28, 0x28);
const SkColor kDefaultToolbarTopSeparator = SkColorSetA(SK_ColorBLACK, 0x40);

#if defined(OS_MACOSX)
const SkColor kDefaultColorFrameVibrancyOverlay =
    SkColorSetA(SK_ColorBLACK, 0x19);
const SkColor kDefaultColorFrameVibrancyOverlayIncognito =
    SkColorSetARGB(0xE6, 0x14, 0x16, 0x18);
const SkColor kDefaultColorToolbarInactive = SkColorSetRGB(0xF6, 0xF6, 0xF6);
const SkColor kDefaultColorToolbarInactiveIncognito =
    SkColorSetRGB(0x2D, 0x2D, 0x2D);
const SkColor kDefaultColorTabBackgroundInactive =
    SkColorSetRGB(0xEC, 0xEC, 0xEC);
const SkColor kDefaultColorTabBackgroundInactiveIncognito =
    SkColorSetRGB(0x28, 0x28, 0x28);
const SkColor kDefaultColorToolbarButtonStroke =
    SkColorSetARGB(0x4B, 0x51, 0x51, 0x51);
const SkColor kDefaultColorToolbarButtonStrokeInactive =
    SkColorSetARGB(0x4B, 0x63, 0x63, 0x63);
const SkColor kDefaultColorToolbarBezel = SkColorSetRGB(0xCC, 0xCC, 0xCC);
const SkColor kDefaultColorToolbarStroke = SkColorSetA(SK_ColorBLACK, 0x4C);
const SkColor kDefaultColorToolbarStrokeInactive =
    SkColorSetRGB(0xA3, 0xA3, 0xA3);
const SkColor kDefaultColorToolbarIncognitoStroke =
    SkColorSetA(SK_ColorBLACK, 0x3F);
const SkColor kDefaultColorToolbarStrokeTheme =
    SkColorSetA(SK_ColorWHITE, 0x66);
const SkColor kDefaultColorToolbarStrokeThemeInactive =
    SkColorSetARGB(0x66, 0x4C, 0x4C, 0x4C);
#endif  // OS_MACOSX
// ----------------------------------------------------------------------------

// Strings used in alignment properties.
constexpr char kAlignmentCenter[] = "center";
constexpr char kAlignmentTop[] = "top";
constexpr char kAlignmentBottom[] = "bottom";
constexpr char kAlignmentLeft[] = "left";
constexpr char kAlignmentRight[] = "right";

// Strings used in background tiling repetition properties.
constexpr char kTilingNoRepeat[] = "no-repeat";
constexpr char kTilingRepeatX[] = "repeat-x";
constexpr char kTilingRepeatY[] = "repeat-y";
constexpr char kTilingRepeat[] = "repeat";

SkColor TintForUnderline(SkColor input) {
  return SkColorSetA(input, SkColorGetA(input) / 3);
}

}  // namespace

// static
int ThemeProperties::StringToAlignment(const std::string& alignment) {
  int alignment_mask = 0;
  for (const std::string& component : base::SplitString(
           alignment, base::kWhitespaceASCII,
           base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (base::LowerCaseEqualsASCII(component, kAlignmentTop))
      alignment_mask |= ALIGN_TOP;
    else if (base::LowerCaseEqualsASCII(component, kAlignmentBottom))
      alignment_mask |= ALIGN_BOTTOM;
    else if (base::LowerCaseEqualsASCII(component, kAlignmentLeft))
      alignment_mask |= ALIGN_LEFT;
    else if (base::LowerCaseEqualsASCII(component, kAlignmentRight))
      alignment_mask |= ALIGN_RIGHT;
  }
  return alignment_mask;
}

// static
int ThemeProperties::StringToTiling(const std::string& tiling) {
  if (base::LowerCaseEqualsASCII(tiling, kTilingRepeatX))
    return REPEAT_X;
  if (base::LowerCaseEqualsASCII(tiling, kTilingRepeatY))
    return REPEAT_Y;
  if (base::LowerCaseEqualsASCII(tiling, kTilingRepeat))
    return REPEAT;
  // NO_REPEAT is the default choice.
  return NO_REPEAT;
}

// static
std::string ThemeProperties::AlignmentToString(int alignment) {
  // Convert from an AlignmentProperty back into a string.
  std::string vertical_string(kAlignmentCenter);
  std::string horizontal_string(kAlignmentCenter);

  if (alignment & ALIGN_TOP)
    vertical_string = kAlignmentTop;
  else if (alignment & ALIGN_BOTTOM)
    vertical_string = kAlignmentBottom;

  if (alignment & ALIGN_LEFT)
    horizontal_string = kAlignmentLeft;
  else if (alignment & ALIGN_RIGHT)
    horizontal_string = kAlignmentRight;

  return horizontal_string + " " + vertical_string;
}

// static
std::string ThemeProperties::TilingToString(int tiling) {
  // Convert from a TilingProperty back into a string.
  if (tiling == REPEAT_X)
    return kTilingRepeatX;
  if (tiling == REPEAT_Y)
    return kTilingRepeatY;
  if (tiling == REPEAT)
    return kTilingRepeat;
  return kTilingNoRepeat;
}

// static
color_utils::HSL ThemeProperties::GetDefaultTint(int id, bool otr) {
  switch (id) {
    case TINT_FRAME:
      return otr ? kDefaultTintFrameIncognito : kDefaultTintFrame;
    case TINT_FRAME_INACTIVE:
      return otr ? kDefaultTintFrameIncognitoInactive
                 : kDefaultTintFrameInactive;
    case TINT_BUTTONS:
      return otr ? kDefaultTintButtonsIncognito : kDefaultTintButtons;
    case TINT_BACKGROUND_TAB:
      return kDefaultTintBackgroundTab;
    case TINT_FRAME_INCOGNITO:
    case TINT_FRAME_INCOGNITO_INACTIVE:
      NOTREACHED() << "These values should be queried via their respective "
                      "non-incognito equivalents and an appropriate |otr| "
                      "value.";
    default:
      return {-1, -1, -1};
  }
}

// static
SkColor ThemeProperties::GetDefaultColor(int id, bool otr) {
  switch (id) {
    // Properties stored in theme pack.
    case COLOR_FRAME:
      return otr ? kDefaultColorFrameIncognito : kDefaultColorFrame;
    case COLOR_FRAME_INACTIVE:
#if defined(OS_MACOSX)
      if (otr)
        return kDefaultColorFrameIncognitoInactiveMac;
#endif
      return color_utils::HSLShift(
          GetDefaultColor(ThemeProperties::COLOR_FRAME, otr),
          GetDefaultTint(ThemeProperties::TINT_FRAME_INACTIVE, false));
    case COLOR_TOOLBAR:
      return otr ? kDefaultColorToolbarIncognito : kDefaultColorToolbar;
    case COLOR_TAB_TEXT:
    case COLOR_BOOKMARK_TEXT:
      return otr ? kDefaultColorToolbarTextIncognito : kDefaultColorToolbarText;
    case COLOR_BACKGROUND_TAB_TEXT:
      return otr ? kDefaultColorBackgroundTabTextIncognito
                 : kDefaultColorBackgroundTabText;
    case COLOR_NTP_BACKGROUND:
      return kDefaultColorNTPBackground;
    case COLOR_NTP_TEXT:
      return kDefaultColorNTPText;
    case COLOR_NTP_LINK:
      return kDefaultColorNTPLink;
    case COLOR_NTP_LINK_UNDERLINE:
      return TintForUnderline(kDefaultColorNTPLink);
    case COLOR_NTP_HEADER:
      return kDefaultColorNTPHeader;
    case COLOR_NTP_SECTION:
      return kDefaultColorNTPSection;
    case COLOR_NTP_SECTION_TEXT:
      return kDefaultColorNTPSectionText;
    case COLOR_NTP_SECTION_LINK:
      return kDefaultColorNTPSectionLink;
    case COLOR_NTP_SECTION_LINK_UNDERLINE:
      return TintForUnderline(kDefaultColorNTPSectionLink);
    case COLOR_BUTTON_BACKGROUND:
      return kDefaultColorButtonBackground;

    // Properties not stored in theme pack.
    case COLOR_CONTROL_BACKGROUND:
      return kDefaultColorControlBackground;
    case COLOR_BOOKMARK_BAR_INSTRUCTIONS_TEXT:
      return otr ? kDefaultColorBookmarkInstructionsTextIncognito
                 : kDefaultColorBookmarkInstructionsText;
    case COLOR_TOOLBAR_BOTTOM_SEPARATOR:
    case COLOR_DETACHED_BOOKMARK_BAR_SEPARATOR:
      return otr ? kDefaultDetachedBookmarkBarSeparatorIncognito
                 : kDefaultDetachedBookmarkBarSeparator;
    case COLOR_DETACHED_BOOKMARK_BAR_BACKGROUND:
      return otr ? kDefaultDetachedBookmarkBarBackgroundIncognito
                 : kDefaultDetachedBookmarkBarBackground;
    case COLOR_TOOLBAR_TOP_SEPARATOR:
    case COLOR_TOOLBAR_TOP_SEPARATOR_INACTIVE:
      return kDefaultToolbarTopSeparator;
#if defined(OS_MACOSX)
    case COLOR_FRAME_VIBRANCY_OVERLAY:
      return otr ? kDefaultColorFrameVibrancyOverlayIncognito
                 : kDefaultColorFrameVibrancyOverlay;
    case COLOR_TOOLBAR_INACTIVE:
      return otr ? kDefaultColorToolbarInactiveIncognito
                 : kDefaultColorToolbarInactive;
    case COLOR_BACKGROUND_TAB_INACTIVE:
      return otr ? kDefaultColorTabBackgroundInactiveIncognito
                 : kDefaultColorTabBackgroundInactive;
    case COLOR_TOOLBAR_BUTTON_STROKE:
      return kDefaultColorToolbarButtonStroke;
    case COLOR_TOOLBAR_BUTTON_STROKE_INACTIVE:
      return kDefaultColorToolbarButtonStrokeInactive;
    case COLOR_TOOLBAR_BEZEL:
      return kDefaultColorToolbarBezel;
    case COLOR_TOOLBAR_STROKE:
      return otr ? kDefaultColorToolbarIncognitoStroke
                 : kDefaultColorToolbarStroke;
    case COLOR_TOOLBAR_STROKE_INACTIVE:
      return kDefaultColorToolbarStrokeInactive;
    case COLOR_TOOLBAR_STROKE_THEME:
      return kDefaultColorToolbarStrokeTheme;
    case COLOR_TOOLBAR_STROKE_THEME_INACTIVE:
      return kDefaultColorToolbarStrokeThemeInactive;
#endif
#if defined(OS_WIN)
    case COLOR_ACCENT_BORDER:
      NOTREACHED();
      return gfx::kPlaceholderColor;
#endif

    case COLOR_FRAME_INCOGNITO:
    case COLOR_FRAME_INCOGNITO_INACTIVE:
      NOTREACHED() << "These values should be queried via their respective "
                      "non-incognito equivalents and an appropriate |otr| "
                      "value.";
      return gfx::kPlaceholderColor;
  }

  return gfx::kPlaceholderColor;
}
