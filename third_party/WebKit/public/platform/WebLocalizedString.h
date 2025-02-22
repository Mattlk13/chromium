/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebLocalizedString_h
#define WebLocalizedString_h

namespace blink {

struct WebLocalizedString {
  enum Name {
    AXAMPMFieldText,
    AXCalendarShowMonthSelector,
    AXCalendarShowNextMonth,
    AXCalendarShowPreviousMonth,
    AXCalendarWeekDescription,
    AXDayOfMonthFieldText,
    AXHeadingText,  // Deprecated.
    AXHourFieldText,
    AXImageMapText,  // Deprecated.
    AXLinkText,        // Deprecated.
    AXListMarkerText,  // Deprecated.
    AXMediaAudioElement,
    AXMediaAudioElementHelp,
    AXMediaAudioSliderHelp,
    AXMediaCastOffButton,
    AXMediaCastOffButtonHelp,
    AXMediaCastOnButton,
    AXMediaCastOnButtonHelp,
    AXMediaCurrentTimeDisplay,
    AXMediaCurrentTimeDisplayHelp,
    AXMediaDefault,
    AXMediaDownloadButton,
    AXMediaEnterFullscreenButton,
    AXMediaEnterFullscreenButtonHelp,
    AXMediaExitFullscreenButton,
    AXMediaExitFullscreenButtonHelp,
    AXMediaHideClosedCaptionsButton,
    AXMediaHideClosedCaptionsButtonHelp,
    AXMediaMuteButton,
    AXMediaMuteButtonHelp,
    AXMediaOverflowButton,
    AXMediaOverflowButtonHelp,
    AXMediaPauseButton,
    AXMediaPauseButtonHelp,
    AXMediaPlayButton,
    AXMediaPlayButtonHelp,
    AXMediaShowClosedCaptionsButton,
    AXMediaShowClosedCaptionsButtonHelp,
    AXMediaSlider,           // Deprecated.
    AXMediaSliderThumb,      // Deprecated.
    AXMediaSliderThumbHelp,  // Deprecated.
    AXMediaStatusDisplay,
    AXMediaStatusDisplayHelp,
    AXMediaTimeRemainingDisplay,
    AXMediaTimeRemainingDisplayHelp,
    AXMediaUnMuteButton,
    AXMediaUnMuteButtonHelp,
    AXMediaVideoElement,
    AXMediaVideoElementHelp,
    AXMediaVideoSliderHelp,
    AXMillisecondFieldText,
    AXMinuteFieldText,
    AXMonthFieldText,
    AXSecondFieldText,
    AXWebAreaText,  // Deprecated.
    AXWeekOfYearFieldText,
    AXYearFieldText,
    BlockedPluginText,
    CalendarClear,
    CalendarToday,
    DateFormatDayInMonthLabel,
    DateFormatMonthLabel,
    DateFormatYearLabel,
    DetailsLabel,
    DownloadButtonLabel,
    FileButtonChooseFileLabel,
    FileButtonChooseMultipleFilesLabel,
    FileButtonNoFileSelectedLabel,
    InputElementAltText,
    MissingPluginText,
    MultipleFileUploadText,
    OtherColorLabel,
    OtherDateLabel,
    OtherMonthLabel,
    OtherTimeLabel,
    OtherWeekLabel,
    OverflowMenuCaptions,
    OverflowMenuCast,
    OverflowMenuEnterFullscreen,
    OverflowMenuExitFullscreen,
    OverflowMenuStopCast,
    OverflowMenuMute,
    OverflowMenuUnmute,
    OverflowMenuPlay,
    OverflowMenuPause,
    OverflowMenuDownload,
    // PlaceholderForDayOfMonthField is for day placeholder text, e.g.
    // "dd", for date field used in multiple fields "date", "datetime", and
    // "datetime-local" input UI instead of "--".
    PlaceholderForDayOfMonthField,
    // PlaceholderForfMonthField is for month placeholder text, e.g.
    // "mm", for month field used in multiple fields "date", "datetime", and
    // "datetime-local" input UI instead of "--".
    PlaceholderForMonthField,
    // PlaceholderForYearField is for year placeholder text, e.g. "yyyy",
    // for year field used in multiple fields "date", "datetime", and
    // "datetime-local" input UI instead of "----".
    PlaceholderForYearField,
    ResetButtonDefaultLabel,
    SearchableIndexIntroduction,
    SearchMenuClearRecentSearchesText,  // Deprecated.
    SearchMenuNoRecentSearchesText,     // Deprecated.
    SearchMenuRecentSearchesText,       // Deprecated.
    SelectMenuListText,
    SubmitButtonDefaultLabel,
    TextTracksNoLabel,
    TextTracksOff,
    ThisMonthButtonLabel,
    ThisWeekButtonLabel,
    ValidationBadInputForNumber,
    ValidationBadInputForDateTime,
    ValidationPatternMismatch,
    ValidationRangeOverflow,
    ValidationRangeOverflowDateTime,
    ValidationRangeUnderflow,
    ValidationRangeUnderflowDateTime,
    ValidationStepMismatch,
    ValidationStepMismatchCloseToLimit,
    ValidationTooLong,
    ValidationTooShort,
    ValidationTypeMismatch,
    ValidationTypeMismatchForEmail,
    ValidationTypeMismatchForEmailEmpty,
    ValidationTypeMismatchForEmailEmptyDomain,
    ValidationTypeMismatchForEmailEmptyLocal,
    ValidationTypeMismatchForEmailInvalidDomain,
    ValidationTypeMismatchForEmailInvalidDots,
    ValidationTypeMismatchForEmailInvalidLocal,
    ValidationTypeMismatchForEmailNoAtSign,
    ValidationTypeMismatchForMultipleEmail,
    ValidationTypeMismatchForURL,
    ValidationValueMissing,
    ValidationValueMissingForCheckbox,
    ValidationValueMissingForFile,
    ValidationValueMissingForMultipleFile,
    ValidationValueMissingForRadio,
    ValidationValueMissingForSelect,
    WeekFormatTemplate,
    WeekNumberLabel,
  };
};

}  // namespace blink

#endif
