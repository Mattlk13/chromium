// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/Deprecation.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/workers/WorkerOrWorkletGlobalScope.h"

namespace {

enum Milestone {
  M56,
  M57,
  M58,
  M59,
};

const char* milestoneString(Milestone milestone) {
  // These are the Estimated Stable Dates:
  // https://www.chromium.org/developers/calendar

  switch (milestone) {
    case M56:
      return "M56, around January 2017";
    case M57:
      return "M57, around March 2017";
    case M58:
      return "M58, around April 2017";
    case M59:
      return "M59, around June 2017";
  }

  ASSERT_NOT_REACHED();
  return nullptr;
}

String replacedBy(const char* feature, const char* replacement) {
  return String::format("%s is deprecated. Please use %s instead.", feature,
                        replacement);
}

String willBeRemoved(const char* feature,
                     Milestone milestone,
                     const char* details) {
  return String::format(
      "%s is deprecated and will be removed in %s. See "
      "https://www.chromestatus.com/features/%s for more details.",
      feature, milestoneString(milestone), details);
}

String replacedWillBeRemoved(const char* feature,
                             const char* replacement,
                             Milestone milestone,
                             const char* details) {
  return String::format(
      "%s is deprecated and will be removed in %s. Please use %s instead. See "
      "https://www.chromestatus.com/features/%s for more details.",
      feature, milestoneString(milestone), replacement, details);
}

}  // anonymous namespace

namespace blink {

Deprecation::Deprecation() : m_muteCount(0) {
  m_cssPropertyDeprecationBits.ensureSize(lastUnresolvedCSSProperty + 1);
}

Deprecation::~Deprecation() {}

void Deprecation::clearSuppression() {
  m_cssPropertyDeprecationBits.clearAll();
}

void Deprecation::muteForInspector() {
  m_muteCount++;
}

void Deprecation::unmuteForInspector() {
  m_muteCount--;
}

void Deprecation::suppress(CSSPropertyID unresolvedProperty) {
  DCHECK(isCSSPropertyIDWithName(unresolvedProperty));
  m_cssPropertyDeprecationBits.quickSet(unresolvedProperty);
}

bool Deprecation::isSuppressed(CSSPropertyID unresolvedProperty) {
  DCHECK(isCSSPropertyIDWithName(unresolvedProperty));
  return m_cssPropertyDeprecationBits.quickGet(unresolvedProperty);
}

void Deprecation::warnOnDeprecatedProperties(const LocalFrame* frame,
                                             CSSPropertyID unresolvedProperty) {
  FrameHost* host = frame ? frame->host() : nullptr;
  if (!host || host->deprecation().m_muteCount ||
      host->deprecation().isSuppressed(unresolvedProperty))
    return;

  String message = deprecationMessage(unresolvedProperty);
  if (!message.isEmpty()) {
    host->deprecation().suppress(unresolvedProperty);
    ConsoleMessage* consoleMessage = ConsoleMessage::create(
        DeprecationMessageSource, WarningMessageLevel, message);
    frame->console().addMessage(consoleMessage);
  }
}

String Deprecation::deprecationMessage(CSSPropertyID unresolvedProperty) {
  switch (unresolvedProperty) {
    case CSSPropertyAliasMotionOffset:
      return replacedWillBeRemoved("motion-offset", "offset-distance", M58,
                                   "6390764217040896");
    case CSSPropertyAliasMotionRotation:
      return replacedWillBeRemoved("motion-rotation", "offset-rotate", M58,
                                   "6390764217040896");
    case CSSPropertyAliasMotionPath:
      return replacedWillBeRemoved("motion-path", "offset-path", M58,
                                   "6390764217040896");
    case CSSPropertyMotion:
      return replacedWillBeRemoved("motion", "offset", M58, "6390764217040896");
    case CSSPropertyOffsetRotation:
      return replacedWillBeRemoved("offset-rotation", "offset-rotate", M58,
                                   "6390764217040896");

    default:
      return emptyString();
  }
}

void Deprecation::countDeprecation(const LocalFrame* frame,
                                   UseCounter::Feature feature) {
  if (!frame)
    return;
  FrameHost* host = frame->host();
  if (!host || host->deprecation().m_muteCount)
    return;

  if (!host->useCounter().hasRecordedMeasurement(feature)) {
    host->useCounter().recordMeasurement(feature);
    ASSERT(!deprecationMessage(feature).isEmpty());
    ConsoleMessage* consoleMessage =
        ConsoleMessage::create(DeprecationMessageSource, WarningMessageLevel,
                               deprecationMessage(feature));
    frame->console().addMessage(consoleMessage);
  }
}

void Deprecation::countDeprecation(ExecutionContext* context,
                                   UseCounter::Feature feature) {
  if (!context)
    return;
  if (context->isDocument()) {
    Deprecation::countDeprecation(*toDocument(context), feature);
    return;
  }
  if (context->isWorkerOrWorkletGlobalScope())
    toWorkerOrWorkletGlobalScope(context)->countDeprecation(feature);
}

void Deprecation::countDeprecation(const Document& document,
                                   UseCounter::Feature feature) {
  Deprecation::countDeprecation(document.frame(), feature);
}

void Deprecation::countDeprecationCrossOriginIframe(
    const LocalFrame* frame,
    UseCounter::Feature feature) {
  // Check to see if the frame can script into the top level document.
  SecurityOrigin* securityOrigin =
      frame->securityContext()->getSecurityOrigin();
  Frame* top = frame->tree().top();
  if (top &&
      !securityOrigin->canAccess(top->securityContext()->getSecurityOrigin()))
    countDeprecation(frame, feature);
}

void Deprecation::countDeprecationCrossOriginIframe(
    const Document& document,
    UseCounter::Feature feature) {
  LocalFrame* frame = document.frame();
  if (!frame)
    return;
  countDeprecationCrossOriginIframe(frame, feature);
}

String Deprecation::deprecationMessage(UseCounter::Feature feature) {
  switch (feature) {
    // Quota
    case UseCounter::PrefixedStorageInfo:
      return replacedBy("'window.webkitStorageInfo'",
                        "'navigator.webkitTemporaryStorage' or "
                        "'navigator.webkitPersistentStorage'");

    case UseCounter::ConsoleMarkTimeline:
      return replacedBy("'console.markTimeline'", "'console.timeStamp'");

    case UseCounter::CSSStyleSheetInsertRuleOptionalArg:
      return "Calling CSSStyleSheet.insertRule() with one argument is "
             "deprecated. Please pass the index argument as well: "
             "insertRule(x, 0).";

    case UseCounter::MapNameMatchingASCIICaseless:
    case UseCounter::MapNameMatchingUnicodeLower:
      return willBeRemoved("Case-insensitive matching for usemap attribute",
                           M58, "5760965337415680");

    case UseCounter::PrefixedVideoSupportsFullscreen:
      return replacedBy("'HTMLVideoElement.webkitSupportsFullscreen'",
                        "'Document.fullscreenEnabled'");

    case UseCounter::PrefixedVideoDisplayingFullscreen:
      return replacedBy("'HTMLVideoElement.webkitDisplayingFullscreen'",
                        "'Document.fullscreenElement'");

    case UseCounter::PrefixedVideoEnterFullscreen:
      return replacedBy("'HTMLVideoElement.webkitEnterFullscreen()'",
                        "'Element.requestFullscreen()'");

    case UseCounter::PrefixedVideoExitFullscreen:
      return replacedBy("'HTMLVideoElement.webkitExitFullscreen()'",
                        "'Document.exitFullscreen()'");

    case UseCounter::PrefixedVideoEnterFullScreen:
      return replacedBy("'HTMLVideoElement.webkitEnterFullScreen()'",
                        "'Element.requestFullscreen()'");

    case UseCounter::PrefixedVideoExitFullScreen:
      return replacedBy("'HTMLVideoElement.webkitExitFullScreen()'",
                        "'Document.exitFullscreen()'");

    case UseCounter::PrefixedRequestAnimationFrame:
      return "'webkitRequestAnimationFrame' is vendor-specific. Please use the "
             "standard 'requestAnimationFrame' instead.";

    case UseCounter::PrefixedCancelAnimationFrame:
      return "'webkitCancelAnimationFrame' is vendor-specific. Please use the "
             "standard 'cancelAnimationFrame' instead.";

    case UseCounter::PictureSourceSrc:
      return "<source src> with a <picture> parent is invalid and therefore "
             "ignored. Please use <source srcset> instead.";

    case UseCounter::ConsoleTimeline:
      return replacedBy("'console.timeline'", "'console.time'");

    case UseCounter::ConsoleTimelineEnd:
      return replacedBy("'console.timelineEnd'", "'console.timeEnd'");

    case UseCounter::XMLHttpRequestSynchronousInNonWorkerOutsideBeforeUnload:
      return "Synchronous XMLHttpRequest on the main thread is deprecated "
             "because of its detrimental effects to the end user's experience. "
             "For more help, check https://xhr.spec.whatwg.org/.";

    case UseCounter::GetMatchedCSSRules:
      return "'getMatchedCSSRules()' is deprecated. For more help, check "
             "https://code.google.com/p/chromium/issues/detail?id=437569#c2";

    case UseCounter::PrefixedWindowURL:
      return replacedBy("'webkitURL'", "'URL'");

    case UseCounter::RangeExpand:
      return replacedBy("'Range.expand()'", "'Selection.modify()'");

    // Powerful features on insecure origins (https://goo.gl/rStTGz)
    case UseCounter::DeviceMotionInsecureOrigin:
      return "The devicemotion event is deprecated on insecure origins, and "
             "support will be removed in the future. You should consider "
             "switching your application to a secure origin, such as HTTPS. "
             "See https://goo.gl/rStTGz for more details.";

    case UseCounter::DeviceOrientationInsecureOrigin:
      return "The deviceorientation event is deprecated on insecure origins, "
             "and support will be removed in the future. You should consider "
             "switching your application to a secure origin, such as HTTPS. "
             "See https://goo.gl/rStTGz for more details.";

    case UseCounter::DeviceOrientationAbsoluteInsecureOrigin:
      return "The deviceorientationabsolute event is deprecated on insecure "
             "origins, and support will be removed in the future. You should "
             "consider switching your application to a secure origin, such as "
             "HTTPS. See https://goo.gl/rStTGz for more details.";

    case UseCounter::GeolocationInsecureOrigin:
    case UseCounter::GeolocationInsecureOriginIframe:
      return "getCurrentPosition() and watchPosition() no longer work on "
             "insecure origins. To use this feature, you should consider "
             "switching your application to a secure origin, such as HTTPS. "
             "See https://goo.gl/rStTGz for more details.";

    case UseCounter::GeolocationInsecureOriginDeprecatedNotRemoved:
    case UseCounter::GeolocationInsecureOriginIframeDeprecatedNotRemoved:
      return "getCurrentPosition() and watchPosition() are deprecated on "
             "insecure origins. To use this feature, you should consider "
             "switching your application to a secure origin, such as HTTPS. "
             "See https://goo.gl/rStTGz for more details.";

    case UseCounter::GetUserMediaInsecureOrigin:
    case UseCounter::GetUserMediaInsecureOriginIframe:
      return "getUserMedia() no longer works on insecure origins. To use this "
             "feature, you should consider switching your application to a "
             "secure origin, such as HTTPS. See https://goo.gl/rStTGz for more "
             "details.";

    case UseCounter::EncryptedMediaInsecureOrigin:
      return String::format(
          "Using requestMediaKeySystemAccess() on insecure origins is "
          "deprecated and will be removed in %s. You should consider "
          "switching your application to a secure origin, such as HTTPS. See "
          "https://goo.gl/rStTGz for more details.",
          milestoneString(M58));

    case UseCounter::MediaSourceAbortRemove:
      return "Using SourceBuffer.abort() to abort remove()'s asynchronous "
             "range removal is deprecated due to specification change. Support "
             "will be removed in the future. You should instead await "
             "'updateend'. abort() is intended to only abort an asynchronous "
             "media append or reset parser state. See "
             "https://www.chromestatus.com/features/6107495151960064 for more "
             "details.";
    case UseCounter::MediaSourceDurationTruncatingBuffered:
      return "Setting MediaSource.duration below the highest presentation "
             "timestamp of any buffered coded frames is deprecated due to "
             "specification change. Support for implicit removal of truncated "
             "buffered media will be removed in the future. You should instead "
             "perform explicit remove(newDuration, oldDuration) on all "
             "sourceBuffers, where newDuration < oldDuration. See "
             "https://www.chromestatus.com/features/6107495151960064 for more "
             "details.";

    case UseCounter::ApplicationCacheManifestSelectInsecureOrigin:
    case UseCounter::ApplicationCacheAPIInsecureOrigin:
      return "Use of the Application Cache is deprecated on insecure origins. "
             "Support will be removed in the future. You should consider "
             "switching your application to a secure origin, such as HTTPS. "
             "See https://goo.gl/rStTGz for more details.";

    case UseCounter::ElementCreateShadowRootMultiple:
      return "Calling Element.createShadowRoot() for an element which already "
             "hosts a shadow root is deprecated. See "
             "https://www.chromestatus.com/features/4668884095336448 for more "
             "details.";

    case UseCounter::CSSDeepCombinator:
      return "/deep/ combinator is deprecated. See "
             "https://www.chromestatus.com/features/6750456638341120 for more "
             "details.";

    case UseCounter::CSSSelectorPseudoShadow:
      return "::shadow pseudo-element is deprecated. See "
             "https://www.chromestatus.com/features/6750456638341120 for more "
             "details.";

    case UseCounter::PrefixedPerformanceClearResourceTimings:
      return replacedBy("'Performance.webkitClearResourceTimings'",
                        "'Performance.clearResourceTimings'");

    case UseCounter::PrefixedPerformanceSetResourceTimingBufferSize:
      return replacedBy("'Performance.webkitSetResourceTimingBufferSize'",
                        "'Performance.setResourceTimingBufferSize'");

    case UseCounter::PrefixedPerformanceResourceTimingBufferFull:
      return replacedBy("'Performance.onwebkitresourcetimingbufferfull'",
                        "'Performance.onresourcetimingbufferfull'");

    case UseCounter::EncryptedMediaAllSelectedContentTypesMissingCodecs:
      return String::format(
          "EME requires that contentType strings accepted by "
          "requestMediaKeySystemAccess() include codecs. Non-standard support "
          "for contentType strings without codecs will be removed in %s. "
          "Please specify the desired codec(s) as part of the contentType.",
          milestoneString(M58));

    case UseCounter::EncryptedMediaCapabilityNotProvided:
      return String::format(
          "EME requires that one of 'audioCapabilities' and "
          "'videoCapabilities' must be non-empty. Non-standard support for "
          "this will be removed in %s. Please specify at least one valid "
          "capability for 'audioCapabilities' or 'videoCapabilities'.",
          milestoneString(M58));

    case UseCounter::VRDeprecatedFieldOfView:
      return replacedBy("VREyeParameters.fieldOfView",
                        "projection matrices provided by VRFrameData");

    case UseCounter::VRDeprecatedGetPose:
      return replacedBy("VRDisplay.getPose()", "VRDisplay.getFrameData()");

    case UseCounter::HTMLEmbedElementLegacyCall:
      return willBeRemoved("HTMLEmbedElement legacy caller", M58,
                           "5715026367217664");

    case UseCounter::HTMLObjectElementLegacyCall:
      return willBeRemoved("HTMLObjectElement legacy caller", M58,
                           "5715026367217664");
    case UseCounter::
        ServiceWorkerRespondToNavigationRequestWithRedirectedResponse:
      return String::format(
          "The service worker responded to the navigation request with a "
          "redirected response. This will result in an error in %s.",
          milestoneString(M59));

    // Features that aren't deprecated don't have a deprecation message.
    default:
      return String();
  }
}

}  // namespace blink
