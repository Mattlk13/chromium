// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/DevToolsEmulator.h"

#include "core/fetch/MemoryCache.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/input/EventHandler.h"
#include "core/page/Page.h"
#include "core/style/ComputedStyle.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/FloatSize.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSize.h"
#include "public/platform/WebLayerTreeView.h"
#include "web/WebInputEventConversion.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebSettingsImpl.h"
#include "web/WebViewImpl.h"
#include "wtf/PtrUtil.h"

namespace {

static float calculateDeviceScaleAdjustment(int width,
                                            int height,
                                            float deviceScaleFactor) {
  // Chromium on Android uses a device scale adjustment for fonts used in text
  // autosizing for improved legibility. This function computes this adjusted
  // value for text autosizing.
  // For a description of the Android device scale adjustment algorithm, see:
  // chrome/browser/chrome_content_browser_client.cc,
  // GetDeviceScaleAdjustment(...)
  if (!width || !height || !deviceScaleFactor)
    return 1;

  static const float kMinFSM = 1.05f;
  static const int kWidthForMinFSM = 320;
  static const float kMaxFSM = 1.3f;
  static const int kWidthForMaxFSM = 800;

  float minWidth = std::min(width, height) / deviceScaleFactor;
  if (minWidth <= kWidthForMinFSM)
    return kMinFSM;
  if (minWidth >= kWidthForMaxFSM)
    return kMaxFSM;

  // The font scale multiplier varies linearly between kMinFSM and kMaxFSM.
  float ratio = static_cast<float>(minWidth - kWidthForMinFSM) /
                (kWidthForMaxFSM - kWidthForMinFSM);
  return ratio * (kMaxFSM - kMinFSM) + kMinFSM;
}

}  // namespace

namespace blink {

DevToolsEmulator::DevToolsEmulator(WebViewImpl* webViewImpl)
    : m_webViewImpl(webViewImpl),
      m_deviceMetricsEnabled(false),
      m_emulateMobileEnabled(false),
      m_isOverlayScrollbarsEnabled(false),
      m_isOrientationEventEnabled(false),
      m_isMobileLayoutThemeEnabled(false),
      m_originalDefaultMinimumPageScaleFactor(0),
      m_originalDefaultMaximumPageScaleFactor(0),
      m_embedderTextAutosizingEnabled(
          webViewImpl->page()->settings().textAutosizingEnabled()),
      m_embedderDeviceScaleAdjustment(
          webViewImpl->page()->settings().getDeviceScaleAdjustment()),
      m_embedderPreferCompositingToLCDTextEnabled(
          webViewImpl->page()
              ->settings()
              .getPreferCompositingToLCDTextEnabled()),
      m_embedderViewportStyle(
          webViewImpl->page()->settings().getViewportStyle()),
      m_embedderPluginsEnabled(
          webViewImpl->page()->settings().getPluginsEnabled()),
      m_embedderAvailablePointerTypes(
          webViewImpl->page()->settings().getAvailablePointerTypes()),
      m_embedderPrimaryPointerType(
          webViewImpl->page()->settings().getPrimaryPointerType()),
      m_embedderAvailableHoverTypes(
          webViewImpl->page()->settings().getAvailableHoverTypes()),
      m_embedderPrimaryHoverType(
          webViewImpl->page()->settings().getPrimaryHoverType()),
      m_embedderMainFrameResizesAreOrientationChanges(
          webViewImpl->page()
              ->settings()
              .getMainFrameResizesAreOrientationChanges()),
      m_touchEventEmulationEnabled(false),
      m_doubleTapToZoomEnabled(false),
      m_originalTouchEventFeatureDetectionEnabled(false),
      m_originalDeviceSupportsTouch(false),
      m_originalMaxTouchPoints(0),
      m_embedderScriptEnabled(
          webViewImpl->page()->settings().getScriptEnabled()),
      m_scriptExecutionDisabled(false) {}

DevToolsEmulator::~DevToolsEmulator() {}

DevToolsEmulator* DevToolsEmulator::create(WebViewImpl* webViewImpl) {
  return new DevToolsEmulator(webViewImpl);
}

DEFINE_TRACE(DevToolsEmulator) {}

void DevToolsEmulator::setTextAutosizingEnabled(bool enabled) {
  m_embedderTextAutosizingEnabled = enabled;
  bool emulateMobileEnabled = m_deviceMetricsEnabled && m_emulateMobileEnabled;
  if (!emulateMobileEnabled)
    m_webViewImpl->page()->settings().setTextAutosizingEnabled(enabled);
}

void DevToolsEmulator::setDeviceScaleAdjustment(float deviceScaleAdjustment) {
  m_embedderDeviceScaleAdjustment = deviceScaleAdjustment;
  bool emulateMobileEnabled = m_deviceMetricsEnabled && m_emulateMobileEnabled;
  if (!emulateMobileEnabled)
    m_webViewImpl->page()->settings().setDeviceScaleAdjustment(
        deviceScaleAdjustment);
}

void DevToolsEmulator::setPreferCompositingToLCDTextEnabled(bool enabled) {
  m_embedderPreferCompositingToLCDTextEnabled = enabled;
  bool emulateMobileEnabled = m_deviceMetricsEnabled && m_emulateMobileEnabled;
  if (!emulateMobileEnabled)
    m_webViewImpl->page()->settings().setPreferCompositingToLCDTextEnabled(
        enabled);
}

void DevToolsEmulator::setViewportStyle(WebViewportStyle style) {
  m_embedderViewportStyle = style;
  bool emulateMobileEnabled = m_deviceMetricsEnabled && m_emulateMobileEnabled;
  if (!emulateMobileEnabled)
    m_webViewImpl->page()->settings().setViewportStyle(style);
}

void DevToolsEmulator::setPluginsEnabled(bool enabled) {
  m_embedderPluginsEnabled = enabled;
  bool emulateMobileEnabled = m_deviceMetricsEnabled && m_emulateMobileEnabled;
  if (!emulateMobileEnabled)
    m_webViewImpl->page()->settings().setPluginsEnabled(enabled);
}

void DevToolsEmulator::setScriptEnabled(bool enabled) {
  m_embedderScriptEnabled = enabled;
  if (!m_scriptExecutionDisabled)
    m_webViewImpl->page()->settings().setScriptEnabled(enabled);
}

void DevToolsEmulator::setDoubleTapToZoomEnabled(bool enabled) {
  m_doubleTapToZoomEnabled = enabled;
}

bool DevToolsEmulator::doubleTapToZoomEnabled() const {
  return m_touchEventEmulationEnabled ? true : m_doubleTapToZoomEnabled;
}

void DevToolsEmulator::setMainFrameResizesAreOrientationChanges(bool value) {
  m_embedderMainFrameResizesAreOrientationChanges = value;
  bool emulateMobileEnabled = m_deviceMetricsEnabled && m_emulateMobileEnabled;
  if (!emulateMobileEnabled)
    m_webViewImpl->page()->settings().setMainFrameResizesAreOrientationChanges(
        value);
}

void DevToolsEmulator::setAvailablePointerTypes(int types) {
  m_embedderAvailablePointerTypes = types;
  bool emulateMobileEnabled = m_deviceMetricsEnabled && m_emulateMobileEnabled;
  if (!emulateMobileEnabled)
    m_webViewImpl->page()->settings().setAvailablePointerTypes(types);
}

void DevToolsEmulator::setPrimaryPointerType(PointerType pointerType) {
  m_embedderPrimaryPointerType = pointerType;
  bool emulateMobileEnabled = m_deviceMetricsEnabled && m_emulateMobileEnabled;
  if (!emulateMobileEnabled)
    m_webViewImpl->page()->settings().setPrimaryPointerType(pointerType);
}

void DevToolsEmulator::setAvailableHoverTypes(int types) {
  m_embedderAvailableHoverTypes = types;
  bool emulateMobileEnabled = m_deviceMetricsEnabled && m_emulateMobileEnabled;
  if (!emulateMobileEnabled)
    m_webViewImpl->page()->settings().setAvailableHoverTypes(types);
}

void DevToolsEmulator::setPrimaryHoverType(HoverType hoverType) {
  m_embedderPrimaryHoverType = hoverType;
  bool emulateMobileEnabled = m_deviceMetricsEnabled && m_emulateMobileEnabled;
  if (!emulateMobileEnabled)
    m_webViewImpl->page()->settings().setPrimaryHoverType(hoverType);
}

void DevToolsEmulator::enableDeviceEmulation(
    const WebDeviceEmulationParams& params) {
  if (m_deviceMetricsEnabled && m_emulationParams.viewSize == params.viewSize &&
      m_emulationParams.screenPosition == params.screenPosition &&
      m_emulationParams.deviceScaleFactor == params.deviceScaleFactor &&
      m_emulationParams.offset == params.offset &&
      m_emulationParams.scale == params.scale) {
    return;
  }
  if (m_emulationParams.deviceScaleFactor != params.deviceScaleFactor ||
      !m_deviceMetricsEnabled)
    memoryCache()->evictResources();

  m_emulationParams = params;

  if (!m_deviceMetricsEnabled) {
    m_deviceMetricsEnabled = true;
    if (params.viewSize.width || params.viewSize.height)
      m_webViewImpl->setBackgroundColorOverride(Color::darkGray);
  }

  m_webViewImpl->page()->settings().setDeviceScaleAdjustment(
      calculateDeviceScaleAdjustment(params.viewSize.width,
                                     params.viewSize.height,
                                     params.deviceScaleFactor));

  if (params.screenPosition == WebDeviceEmulationParams::Mobile)
    enableMobileEmulation();
  else
    disableMobileEmulation();

  m_webViewImpl->setCompositorDeviceScaleFactorOverride(
      params.deviceScaleFactor);
  updateRootLayerTransform();
  // TODO(dgozman): mainFrameImpl() is null when it's remote. Figure out how
  // we end up with enabling emulation in this case.
  if (m_webViewImpl->mainFrameImpl()) {
    if (Document* document =
            m_webViewImpl->mainFrameImpl()->frame()->document())
      document->mediaQueryAffectingValueChanged();
  }
}

void DevToolsEmulator::disableDeviceEmulation() {
  if (!m_deviceMetricsEnabled)
    return;

  memoryCache()->evictResources();
  m_deviceMetricsEnabled = false;
  m_webViewImpl->setBackgroundColorOverride(Color::transparent);
  m_webViewImpl->page()->settings().setDeviceScaleAdjustment(
      m_embedderDeviceScaleAdjustment);
  disableMobileEmulation();
  m_webViewImpl->setCompositorDeviceScaleFactorOverride(0.f);
  m_webViewImpl->setPageScaleFactor(1.f);
  updateRootLayerTransform();
  // mainFrameImpl() could be null during cleanup or remote <-> local swap.
  if (m_webViewImpl->mainFrameImpl()) {
    if (Document* document =
            m_webViewImpl->mainFrameImpl()->frame()->document())
      document->mediaQueryAffectingValueChanged();
  }
}

void DevToolsEmulator::enableMobileEmulation() {
  if (m_emulateMobileEnabled)
    return;
  m_emulateMobileEnabled = true;
  m_isOverlayScrollbarsEnabled =
      RuntimeEnabledFeatures::overlayScrollbarsEnabled();
  RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(true);
  m_isOrientationEventEnabled =
      RuntimeEnabledFeatures::orientationEventEnabled();
  RuntimeEnabledFeatures::setOrientationEventEnabled(true);
  m_isMobileLayoutThemeEnabled =
      RuntimeEnabledFeatures::mobileLayoutThemeEnabled();
  RuntimeEnabledFeatures::setMobileLayoutThemeEnabled(true);
  ComputedStyle::invalidateInitialStyle();
  m_webViewImpl->page()->settings().setViewportStyle(WebViewportStyle::Mobile);
  m_webViewImpl->page()->settings().setViewportEnabled(true);
  m_webViewImpl->page()->settings().setViewportMetaEnabled(true);
  m_webViewImpl->page()->frameHost().visualViewport().initializeScrollbars();
  m_webViewImpl->settings()->setShrinksViewportContentToFit(true);
  m_webViewImpl->page()->settings().setTextAutosizingEnabled(true);
  m_webViewImpl->page()->settings().setPreferCompositingToLCDTextEnabled(true);
  m_webViewImpl->page()->settings().setPluginsEnabled(false);
  m_webViewImpl->page()->settings().setAvailablePointerTypes(PointerTypeCoarse);
  m_webViewImpl->page()->settings().setPrimaryPointerType(PointerTypeCoarse);
  m_webViewImpl->page()->settings().setAvailableHoverTypes(HoverTypeOnDemand);
  m_webViewImpl->page()->settings().setPrimaryHoverType(HoverTypeOnDemand);
  m_webViewImpl->page()->settings().setMainFrameResizesAreOrientationChanges(
      true);
  m_webViewImpl->setZoomFactorOverride(1);

  m_originalDefaultMinimumPageScaleFactor =
      m_webViewImpl->defaultMinimumPageScaleFactor();
  m_originalDefaultMaximumPageScaleFactor =
      m_webViewImpl->defaultMaximumPageScaleFactor();
  m_webViewImpl->setDefaultPageScaleLimits(0.25f, 5);
  // TODO(dgozman): mainFrameImpl() is null when it's remote. Figure out how
  // we end up with enabling emulation in this case.
  if (m_webViewImpl->mainFrameImpl())
    m_webViewImpl->mainFrameImpl()->frameView()->layout();
}

void DevToolsEmulator::disableMobileEmulation() {
  if (!m_emulateMobileEnabled)
    return;
  RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(
      m_isOverlayScrollbarsEnabled);
  RuntimeEnabledFeatures::setOrientationEventEnabled(
      m_isOrientationEventEnabled);
  RuntimeEnabledFeatures::setMobileLayoutThemeEnabled(
      m_isMobileLayoutThemeEnabled);
  ComputedStyle::invalidateInitialStyle();
  m_webViewImpl->page()->settings().setViewportEnabled(false);
  m_webViewImpl->page()->settings().setViewportMetaEnabled(false);
  m_webViewImpl->page()->frameHost().visualViewport().initializeScrollbars();
  m_webViewImpl->settings()->setShrinksViewportContentToFit(false);
  m_webViewImpl->page()->settings().setTextAutosizingEnabled(
      m_embedderTextAutosizingEnabled);
  m_webViewImpl->page()->settings().setPreferCompositingToLCDTextEnabled(
      m_embedderPreferCompositingToLCDTextEnabled);
  m_webViewImpl->page()->settings().setViewportStyle(m_embedderViewportStyle);
  m_webViewImpl->page()->settings().setPluginsEnabled(m_embedderPluginsEnabled);
  m_webViewImpl->page()->settings().setAvailablePointerTypes(
      m_embedderAvailablePointerTypes);
  m_webViewImpl->page()->settings().setPrimaryPointerType(
      m_embedderPrimaryPointerType);
  m_webViewImpl->page()->settings().setAvailableHoverTypes(
      m_embedderAvailableHoverTypes);
  m_webViewImpl->page()->settings().setPrimaryHoverType(
      m_embedderPrimaryHoverType);
  m_webViewImpl->page()->settings().setMainFrameResizesAreOrientationChanges(
      m_embedderMainFrameResizesAreOrientationChanges);
  m_webViewImpl->setZoomFactorOverride(0);
  m_emulateMobileEnabled = false;
  m_webViewImpl->setDefaultPageScaleLimits(
      m_originalDefaultMinimumPageScaleFactor,
      m_originalDefaultMaximumPageScaleFactor);
  // mainFrameImpl() could be null during cleanup or remote <-> local swap.
  if (m_webViewImpl->mainFrameImpl())
    m_webViewImpl->mainFrameImpl()->frameView()->layout();
}

float DevToolsEmulator::compositorDeviceScaleFactor() const {
  if (m_deviceMetricsEnabled)
    return m_emulationParams.deviceScaleFactor;
  return m_webViewImpl->page()->deviceScaleFactor();
}

void DevToolsEmulator::forceViewport(const WebFloatPoint& position,
                                     float scale) {
  GraphicsLayer* containerLayer =
      m_webViewImpl->page()->frameHost().visualViewport().containerLayer();
  if (!m_viewportOverride) {
    m_viewportOverride = ViewportOverride();

    // Disable clipping on the visual viewport layer, to ensure the whole area
    // is painted.
    if (containerLayer) {
      m_viewportOverride->originalVisualViewportMasking =
          containerLayer->masksToBounds();
      containerLayer->setMasksToBounds(false);
    }
  }

  m_viewportOverride->position = position;
  m_viewportOverride->scale = scale;

  // Move the correct (scaled) content area to show in the top left of the
  // CompositorFrame via the root transform.
  updateRootLayerTransform();
}

void DevToolsEmulator::resetViewport() {
  if (!m_viewportOverride)
    return;

  bool originalMasking = m_viewportOverride->originalVisualViewportMasking;
  m_viewportOverride = WTF::nullopt;

  GraphicsLayer* containerLayer =
      m_webViewImpl->page()->frameHost().visualViewport().containerLayer();
  if (containerLayer)
    containerLayer->setMasksToBounds(originalMasking);
  updateRootLayerTransform();
}

void DevToolsEmulator::mainFrameScrollOrScaleChanged() {
  // Viewport override has to take current page scale and scroll offset into
  // account. Update the transform if override is active.
  if (m_viewportOverride)
    updateRootLayerTransform();
}

void DevToolsEmulator::applyDeviceEmulationTransform(
    TransformationMatrix* transform) {
  if (m_deviceMetricsEnabled) {
    WebSize offset(m_emulationParams.offset.x, m_emulationParams.offset.y);
    // Scale first, so that translation is unaffected.
    transform->translate(offset.width, offset.height);
    transform->scale(m_emulationParams.scale);
    if (m_webViewImpl->mainFrameImpl())
      m_webViewImpl->mainFrameImpl()->setInputEventsTransformForEmulation(
          offset, m_emulationParams.scale);
  } else {
    if (m_webViewImpl->mainFrameImpl())
      m_webViewImpl->mainFrameImpl()->setInputEventsTransformForEmulation(
          WebSize(0, 0), 1.0);
  }
}

void DevToolsEmulator::applyViewportOverride(TransformationMatrix* transform) {
  if (!m_viewportOverride)
    return;

  // Transform operations follow in reverse application.
  // Last, scale positioned area according to override.
  transform->scale(m_viewportOverride->scale);

  // Translate while taking into account current scroll offset.
  WebSize scrollOffset = m_webViewImpl->mainFrame()->getScrollOffset();
  WebFloatPoint visualOffset = m_webViewImpl->visualViewportOffset();
  float scrollX = scrollOffset.width + visualOffset.x;
  float scrollY = scrollOffset.height + visualOffset.y;
  transform->translate(-m_viewportOverride->position.x + scrollX,
                       -m_viewportOverride->position.y + scrollY);

  // First, reverse page scale, so we don't have to take it into account for
  // calculation of the translation.
  transform->scale(1. / m_webViewImpl->pageScaleFactor());
}

void DevToolsEmulator::updateRootLayerTransform() {
  TransformationMatrix transform;

  // Apply device emulation transform first, so that it is affected by the
  // viewport override.
  applyViewportOverride(&transform);
  applyDeviceEmulationTransform(&transform);
  m_webViewImpl->setDeviceEmulationTransform(transform);
}

WTF::Optional<IntRect> DevToolsEmulator::visibleContentRectForPainting() const {
  if (!m_viewportOverride)
    return WTF::nullopt;
  FloatSize viewportSize(m_webViewImpl->layerTreeView()->getViewportSize());
  viewportSize.scale(1. / compositorDeviceScaleFactor());
  viewportSize.scale(1. / m_viewportOverride->scale);
  return enclosingIntRect(
      FloatRect(m_viewportOverride->position.x, m_viewportOverride->position.y,
                viewportSize.width(), viewportSize.height()));
}

void DevToolsEmulator::setTouchEventEmulationEnabled(bool enabled) {
  if (m_touchEventEmulationEnabled == enabled)
    return;
  if (!m_touchEventEmulationEnabled) {
    m_originalTouchEventFeatureDetectionEnabled =
        RuntimeEnabledFeatures::touchEventFeatureDetectionEnabled();
    m_originalDeviceSupportsTouch =
        m_webViewImpl->page()->settings().getDeviceSupportsTouch();
    m_originalMaxTouchPoints =
        m_webViewImpl->page()->settings().getMaxTouchPoints();
  }
  RuntimeEnabledFeatures::setTouchEventFeatureDetectionEnabled(
      enabled ? true : m_originalTouchEventFeatureDetectionEnabled);
  if (!m_originalDeviceSupportsTouch) {
    if (enabled && m_webViewImpl->mainFrameImpl()) {
      m_webViewImpl->mainFrameImpl()
          ->frame()
          ->eventHandler()
          .clearMouseEventManager();
    }
    m_webViewImpl->page()->settings().setDeviceSupportsTouch(
        enabled ? true : m_originalDeviceSupportsTouch);
    // Currently emulation does not provide multiple touch points.
    m_webViewImpl->page()->settings().setMaxTouchPoints(
        enabled ? 1 : m_originalMaxTouchPoints);
  }
  m_touchEventEmulationEnabled = enabled;
  // TODO(dgozman): mainFrameImpl() check in this class should be unnecessary.
  // It is only needed when we reattach and restore InspectorEmulationAgent,
  // which happens before everything has been setup correctly, and therefore
  // fails during remote -> local main frame transition.
  // We should instead route emulation from browser through the WebViewImpl
  // to the local main frame, and remove InspectorEmulationAgent entirely.
  if (m_webViewImpl->mainFrameImpl())
    m_webViewImpl->mainFrameImpl()->frameView()->layout();
}

void DevToolsEmulator::setScriptExecutionDisabled(
    bool scriptExecutionDisabled) {
  m_scriptExecutionDisabled = scriptExecutionDisabled;
  m_webViewImpl->page()->settings().setScriptEnabled(
      m_scriptExecutionDisabled ? false : m_embedderScriptEnabled);
}

bool DevToolsEmulator::handleInputEvent(const WebInputEvent& inputEvent) {
  Page* page = m_webViewImpl->page();
  if (!page)
    return false;

  // FIXME: This workaround is required for touch emulation on Mac, where
  // compositor-side pinch handling is not enabled. See http://crbug.com/138003.
  bool isPinch = inputEvent.type == WebInputEvent::GesturePinchBegin ||
                 inputEvent.type == WebInputEvent::GesturePinchUpdate ||
                 inputEvent.type == WebInputEvent::GesturePinchEnd;
  if (isPinch && m_touchEventEmulationEnabled) {
    FrameView* frameView = page->deprecatedLocalMainFrame()->view();
    WebGestureEvent scaledEvent = TransformWebGestureEvent(
        frameView, static_cast<const WebGestureEvent&>(inputEvent));
    float pageScaleFactor = page->pageScaleFactor();
    if (scaledEvent.type == WebInputEvent::GesturePinchBegin) {
      WebFloatPoint gesturePosition = scaledEvent.positionInRootFrame();
      m_lastPinchAnchorCss = WTF::wrapUnique(new IntPoint(
          roundedIntPoint(gesturePosition + frameView->getScrollOffset())));
      m_lastPinchAnchorDip =
          WTF::wrapUnique(new IntPoint(flooredIntPoint(gesturePosition)));
      m_lastPinchAnchorDip->scale(pageScaleFactor, pageScaleFactor);
    }
    if (scaledEvent.type == WebInputEvent::GesturePinchUpdate &&
        m_lastPinchAnchorCss) {
      float newPageScaleFactor = pageScaleFactor * scaledEvent.pinchScale();
      IntPoint anchorCss(*m_lastPinchAnchorDip.get());
      anchorCss.scale(1.f / newPageScaleFactor, 1.f / newPageScaleFactor);
      m_webViewImpl->setPageScaleFactor(newPageScaleFactor);
      m_webViewImpl->mainFrame()->setScrollOffset(
          toIntSize(*m_lastPinchAnchorCss.get() - toIntSize(anchorCss)));
    }
    if (scaledEvent.type == WebInputEvent::GesturePinchEnd) {
      m_lastPinchAnchorCss.reset();
      m_lastPinchAnchorDip.reset();
    }
    return true;
  }

  return false;
}

}  // namespace blink
