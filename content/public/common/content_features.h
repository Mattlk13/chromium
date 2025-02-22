// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the content
// module.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
CONTENT_EXPORT extern const base::Feature kAsmJsToWebAssembly;
CONTENT_EXPORT extern const base::Feature kBrotliEncoding;
CONTENT_EXPORT extern const base::Feature kBrowserSideNavigation;
CONTENT_EXPORT extern const base::Feature kCanvas2DImageChromium;
CONTENT_EXPORT extern const base::Feature kCompositeOpaqueFixedPosition;
CONTENT_EXPORT extern const base::Feature kCompositeOpaqueScrollers;
CONTENT_EXPORT extern const base::Feature kCredentialManagementAPI;
CONTENT_EXPORT extern const base::Feature kDefaultEnableGpuRasterization;
CONTENT_EXPORT extern const base::Feature kDocumentWriteEvaluator;
CONTENT_EXPORT extern const base::Feature kExpensiveBackgroundTimerThrottling;
CONTENT_EXPORT extern const base::Feature kFasterLocationReload;
CONTENT_EXPORT extern const base::Feature kFeaturePolicy;
CONTENT_EXPORT extern const base::Feature kFontCacheScaling;
CONTENT_EXPORT extern const base::Feature
    kFramebustingNeedsSameOriginOrUserGesture;
CONTENT_EXPORT extern const base::Feature kGamepadExtensions;
CONTENT_EXPORT extern const base::Feature kGenericSensor;
CONTENT_EXPORT extern const base::Feature kGuestViewCrossProcessFrames;
CONTENT_EXPORT extern const base::Feature kLazyParseCSS;
CONTENT_EXPORT extern const base::Feature kMediaDocumentDownloadButton;
CONTENT_EXPORT extern const base::Feature kMemoryCoordinator;
CONTENT_EXPORT extern const base::Feature kNotificationContentImage;
CONTENT_EXPORT extern const base::Feature kMainThreadBusyScrollIntervention;
CONTENT_EXPORT extern const base::Feature kOptimizeLoadingIPCForSmallResources;
CONTENT_EXPORT extern const base::Feature kOriginTrials;
CONTENT_EXPORT extern const base::Feature kParseHTMLOnMainThread;
CONTENT_EXPORT extern const base::Feature kPassiveDocumentEventListeners;
CONTENT_EXPORT extern const base::Feature kPassiveEventListenersDueToFling;
CONTENT_EXPORT extern const base::Feature kPepper3DImageChromium;
CONTENT_EXPORT extern const base::Feature kPointerEvents;
CONTENT_EXPORT extern const base::Feature kPurgeAndSuspend;
CONTENT_EXPORT extern const base::Feature kRafAlignedMouseInputEvents;
CONTENT_EXPORT extern const base::Feature kRafAlignedTouchInputEvents;
CONTENT_EXPORT extern const base::Feature kRenderingPipelineThrottling;
CONTENT_EXPORT extern const base::Feature kScrollAnchoring;
CONTENT_EXPORT extern const base::Feature kServiceWorkerNavigationPreload;
CONTENT_EXPORT extern const base::Feature kSharedArrayBuffer;
CONTENT_EXPORT extern const base::Feature kSpeculativeLaunchServiceWorker;
CONTENT_EXPORT extern const base::Feature kStaleWhileRevalidate;
CONTENT_EXPORT extern const base::Feature kTimerThrottlingForHiddenFrames;
CONTENT_EXPORT extern const base::Feature kTokenBinding;
CONTENT_EXPORT extern const base::Feature kTouchpadAndWheelScrollLatching;
CONTENT_EXPORT extern const base::Feature kVrShell;
CONTENT_EXPORT extern const base::Feature kWebAssembly;
CONTENT_EXPORT extern const base::Feature kWebGLImageChromium;
CONTENT_EXPORT extern const base::Feature kWebRtcEcdsaDefault;
CONTENT_EXPORT extern const base::Feature kWebRtcHWH264Encoding;
CONTENT_EXPORT extern const base::Feature kWebRtcUseGpuMemoryBufferVideoFrames;
CONTENT_EXPORT extern const base::Feature kWebUsb;
CONTENT_EXPORT
extern const base::Feature kSendBeaconThrowForBlobWithNonSimpleType;

#if defined(OS_ANDROID)
CONTENT_EXPORT extern const base::Feature kImeThread;
CONTENT_EXPORT extern const base::Feature kSeccompSandboxAndroid;
CONTENT_EXPORT extern const base::Feature kServiceWorkerPaymentApps;
CONTENT_EXPORT extern const base::Feature kWebPayments;
#endif  // defined(OS_ANDROID)

#if !defined(OS_ANDROID)
CONTENT_EXPORT extern const base::Feature
    kCrossOriginMediaPlaybackRequiresUserGesture;
#endif  // !defined(OS_ANDROID)

#if defined(OS_WIN)
CONTENT_EXPORT extern const base::Feature kWinSboxDisableExtensionPoints;
#endif  // defined(OS_WIN)

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace features

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_
