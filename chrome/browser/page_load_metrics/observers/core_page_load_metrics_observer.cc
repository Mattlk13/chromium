// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_load_metrics/observers/core_page_load_metrics_observer.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_util.h"
#include "components/rappor/public/rappor_utils.h"
#include "components/rappor/rappor_service_impl.h"
#include "net/http/http_response_headers.h"
#include "ui/base/page_transition_types.h"

namespace {

// The number of buckets in the bitfield histogram. These buckets are described
// in rappor.xml in PageLoad.CoarseTiming.NavigationToFirstContentfulPaint.
// The bucket flag is defined by 1 << bucket_index, and is the bitfield
// representing which timing bucket the page load falls into, i.e. 000010
// would be the bucket flag showing that the page took between 2 and 4 seconds
// to load.
const size_t kNumRapporHistogramBuckets = 6;

uint64_t RapporHistogramBucketIndex(base::TimeDelta time) {
  int64_t seconds = time.InSeconds();
  if (seconds < 2)
    return 0;
  if (seconds < 4)
    return 1;
  if (seconds < 8)
    return 2;
  if (seconds < 16)
    return 3;
  if (seconds < 32)
    return 4;
  return 5;
}

// TODO(bmcquade): If other observers want to log histograms based on load type,
// promote this enum to page_load_metrics_observer.h.
enum PageLoadType {
  LOAD_TYPE_NONE = 0,
  LOAD_TYPE_RELOAD,
  LOAD_TYPE_FORWARD_BACK,
  LOAD_TYPE_NEW_NAVIGATION
};

PageLoadType GetPageLoadType(ui::PageTransition transition) {
  if (transition & ui::PAGE_TRANSITION_FORWARD_BACK) {
    return LOAD_TYPE_FORWARD_BACK;
  }
  if (ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_RELOAD)) {
    return LOAD_TYPE_RELOAD;
  }
  if (ui::PageTransitionIsNewNavigation(transition)) {
    return LOAD_TYPE_NEW_NAVIGATION;
  }
  NOTREACHED() << "Received PageTransition with no matching PageLoadType.";
  return LOAD_TYPE_NONE;
}

void RecordFirstMeaningfulPaintStatus(
    internal::FirstMeaningfulPaintStatus status) {
  UMA_HISTOGRAM_ENUMERATION(internal::kHistogramFirstMeaningfulPaintStatus,
      status, internal::FIRST_MEANINGFUL_PAINT_LAST_ENTRY);
}

}  // namespace

namespace internal {

const char kHistogramDomContentLoaded[] =
    "PageLoad.DocumentTiming.NavigationToDOMContentLoadedEventFired";
const char kBackgroundHistogramDomContentLoaded[] =
    "PageLoad.DocumentTiming.NavigationToDOMContentLoadedEventFired.Background";
const char kHistogramLoad[] =
    "PageLoad.DocumentTiming.NavigationToLoadEventFired";
const char kBackgroundHistogramLoad[] =
    "PageLoad.DocumentTiming.NavigationToLoadEventFired.Background";
const char kHistogramFirstLayout[] =
    "PageLoad.DocumentTiming.NavigationToFirstLayout";
const char kBackgroundHistogramFirstLayout[] =
    "PageLoad.DocumentTiming.NavigationToFirstLayout.Background";
const char kHistogramFirstPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstPaint";
const char kBackgroundHistogramFirstPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstPaint.Background";
const char kHistogramFirstTextPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstTextPaint";
const char kBackgroundHistogramFirstTextPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstTextPaint.Background";
const char kHistogramFirstImagePaint[] =
    "PageLoad.PaintTiming.NavigationToFirstImagePaint";
const char kBackgroundHistogramFirstImagePaint[] =
    "PageLoad.PaintTiming.NavigationToFirstImagePaint.Background";
const char kHistogramFirstContentfulPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint";
const char kBackgroundHistogramFirstContentfulPaint[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.Background";
const char kHistogramFirstMeaningfulPaint[] =
    "PageLoad.Experimental.PaintTiming.NavigationToFirstMeaningfulPaint";
const char kHistogramFirstMeaningfulPaintNoUserInput[] =
    "PageLoad.Experimental.PaintTiming.NavigationToFirstMeaningfulPaint."
    "NoUserInput";
const char kHistogramFirstMeaningfulPaintHadUserInput[] =
    "PageLoad.Experimental.PaintTiming.NavigationToFirstMeaningfulPaint."
    "HadUserInput";
const char kHistogramParseStartToFirstMeaningfulPaint[] =
    "PageLoad.Experimental.PaintTiming.ParseStartToFirstMeaningfulPaint";
const char kHistogramParseStartToFirstContentfulPaint[] =
    "PageLoad.PaintTiming.ParseStartToFirstContentfulPaint";
const char kBackgroundHistogramParseStartToFirstContentfulPaint[] =
    "PageLoad.PaintTiming.ParseStartToFirstContentfulPaint.Background";
const char kHistogramParseStart[] =
    "PageLoad.ParseTiming.NavigationToParseStart";
const char kBackgroundHistogramParseStart[] =
    "PageLoad.ParseTiming.NavigationToParseStart.Background";
const char kHistogramFirstMeaningfulPaintToNetworkStable[] =
    "PageLoad.Experimental.PaintTiming.FirstMeaningfulPaintToNetworkStable";
const char kHistogramParseDuration[] = "PageLoad.ParseTiming.ParseDuration";
const char kBackgroundHistogramParseDuration[] =
    "PageLoad.ParseTiming.ParseDuration.Background";
const char kHistogramParseBlockedOnScriptLoad[] =
    "PageLoad.ParseTiming.ParseBlockedOnScriptLoad";
const char kBackgroundHistogramParseBlockedOnScriptLoad[] =
    "PageLoad.ParseTiming.ParseBlockedOnScriptLoad.Background";
const char kHistogramParseBlockedOnScriptLoadDocumentWrite[] =
    "PageLoad.ParseTiming.ParseBlockedOnScriptLoadFromDocumentWrite";
const char kBackgroundHistogramParseBlockedOnScriptLoadDocumentWrite[] =
    "PageLoad.ParseTiming.ParseBlockedOnScriptLoadFromDocumentWrite."
    "Background";
const char kHistogramParseBlockedOnScriptExecution[] =
    "PageLoad.ParseTiming.ParseBlockedOnScriptExecution";
const char kHistogramParseBlockedOnScriptExecutionDocumentWrite[] =
    "PageLoad.ParseTiming.ParseBlockedOnScriptExecutionFromDocumentWrite";

const char kHistogramFirstContentfulPaintNoStore[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.NoStore";

const char kHistogramLoadTypeFirstContentfulPaintReload[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.LoadType."
    "Reload";
const char kHistogramLoadTypeFirstContentfulPaintReloadByGesture[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.LoadType."
    "Reload.UserGesture";
const char kHistogramLoadTypeFirstContentfulPaintForwardBack[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.LoadType."
    "ForwardBackNavigation";
const char kHistogramLoadTypeFirstContentfulPaintForwardBackNoStore[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.LoadType."
    "ForwardBackNavigation.NoStore";
const char kHistogramLoadTypeFirstContentfulPaintNewNavigation[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.LoadType."
    "NewNavigation";

const char kHistogramLoadTypeParseStartReload[] =
    "PageLoad.ParseTiming.NavigationToParseStart.LoadType.Reload";
const char kHistogramLoadTypeParseStartForwardBack[] =
    "PageLoad.ParseTiming.NavigationToParseStart.LoadType."
    "ForwardBackNavigation";
const char kHistogramLoadTypeParseStartForwardBackNoStore[] =
    "PageLoad.ParseTiming.NavigationToParseStart.LoadType."
    "ForwardBackNavigation.NoStore";
const char kHistogramLoadTypeParseStartNewNavigation[] =
    "PageLoad.ParseTiming.NavigationToParseStart.LoadType.NewNavigation";

const char kHistogramFirstForeground[] =
    "PageLoad.Timing2.NavigationToFirstForeground";

const char kHistogramFailedProvisionalLoad[] =
    "PageLoad.Timing2.NavigationToFailedProvisionalLoad";

const char kHistogramForegroundToFirstPaint[] =
    "PageLoad.PaintTiming.ForegroundToFirstPaint";

const char kHistogramCacheRequestPercentParseStop[] =
    "PageLoad.Experimental.Cache.RequestPercent.ParseStop";
const char kHistogramCacheTotalRequestsParseStop[] =
    "PageLoad.Experimental.Cache.TotalRequests.ParseStop";
const char kHistogramTotalRequestsParseStop[] =
    "PageLoad.Experimental.TotalRequests.ParseStop";

const char kRapporMetricsNameCoarseTiming[] =
    "PageLoad.CoarseTiming.NavigationToFirstContentfulPaint";

const char kRapporMetricsNameFirstMeaningfulPaintNotRecorded[] =
    "PageLoad.Experimental.PaintTiming.FirstMeaningfulPaintNotRecorded";

const char kHistogramFirstContentfulPaintUserInitiated[] =
    "PageLoad.PaintTiming.NavigationToFirstContentfulPaint.UserInitiated";

const char kHistogramFirstMeaningfulPaintStatus[] =
    "PageLoad.Experimental.PaintTiming.FirstMeaningfulPaintStatus";
const char kHistogramFirstMeaningfulPaintSignalStatus2[] =
    "PageLoad.Experimental.PaintTiming.FirstMeaningfulPaintSignalStatus2";

const char kHistogramFirstNonScrollInputAfterFirstPaint[] =
    "PageLoad.InputTiming.NavigationToFirstNonScroll.AfterPaint";
const char kHistogramFirstScrollInputAfterFirstPaint[] =
    "PageLoad.InputTiming.NavigationToFirstScroll.AfterPaint";

const char kHistogramTotalBytes[] = "PageLoad.Experimental.Bytes.Total";
const char kHistogramNetworkBytes[] = "PageLoad.Experimental.Bytes.Network";
const char kHistogramCacheBytes[] = "PageLoad.Experimental.Bytes.Cache";

}  // namespace internal

CorePageLoadMetricsObserver::CorePageLoadMetricsObserver()
    : transition_(ui::PAGE_TRANSITION_LINK),
      was_no_store_main_resource_(false) {}

CorePageLoadMetricsObserver::~CorePageLoadMetricsObserver() {}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
CorePageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle) {
  transition_ = navigation_handle->GetPageTransition();
  navigation_start_ = navigation_handle->NavigationStart();
  const net::HttpResponseHeaders* headers =
      navigation_handle->GetResponseHeaders();
  if (headers) {
    was_no_store_main_resource_ =
        headers->HasHeaderValue("cache-control", "no-store");
  }
  return CONTINUE_OBSERVING;
}

void CorePageLoadMetricsObserver::OnDomContentLoadedEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.dom_content_loaded_event_start, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramDomContentLoaded,
                        timing.dom_content_loaded_event_start.value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramDomContentLoaded,
                        timing.dom_content_loaded_event_start.value());
  }
}

void CorePageLoadMetricsObserver::OnLoadEventStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(timing.load_event_start,
                                                      info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramLoad,
                        timing.load_event_start.value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramLoad,
                        timing.load_event_start.value());
  }
}

void CorePageLoadMetricsObserver::OnFirstLayout(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(timing.first_layout,
                                                      info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstLayout,
                        timing.first_layout.value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstLayout,
                        timing.first_layout.value());
  }
}

void CorePageLoadMetricsObserver::OnFirstPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  first_paint_ = navigation_start_ + timing.first_paint.value();
  if (WasStartedInForegroundOptionalEventInForeground(timing.first_paint,
                                                      info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstPaint,
                        timing.first_paint.value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstPaint,
                        timing.first_paint.value());
  }

  // Record the time to first paint for pages which were:
  // - Opened in the background.
  // - Moved to the foreground prior to the first paint.
  // - Not moved back to the background prior to the first paint.
  if (!info.started_in_foreground && info.first_foreground_time &&
      info.first_foreground_time.value() <= timing.first_paint.value() &&
      (!info.first_background_time ||
       timing.first_paint.value() <= info.first_background_time.value())) {
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramForegroundToFirstPaint,
        timing.first_paint.value() - info.first_foreground_time.value());
  }
}

void CorePageLoadMetricsObserver::OnFirstTextPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(timing.first_text_paint,
                                                      info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstTextPaint,
                        timing.first_text_paint.value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstTextPaint,
                        timing.first_text_paint.value());
  }
}

void CorePageLoadMetricsObserver::OnFirstImagePaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(timing.first_image_paint,
                                                      info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstImagePaint,
                        timing.first_image_paint.value());
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstImagePaint,
                        timing.first_image_paint.value());
  }
}

void CorePageLoadMetricsObserver::OnFirstContentfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.first_contentful_paint, info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstContentfulPaint,
                        timing.first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramParseStartToFirstContentfulPaint,
        timing.first_contentful_paint.value() - timing.parse_start.value());

    if (was_no_store_main_resource_) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstContentfulPaintNoStore,
                          timing.first_contentful_paint.value());
    }

    // TODO(bmcquade): consider adding a histogram that uses
    // UserInputInfo.user_input_event.
    if (info.user_initiated_info.browser_initiated ||
        info.user_initiated_info.user_gesture) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstContentfulPaintUserInitiated,
                          timing.first_contentful_paint.value());
    }

    if (timing.style_sheet_timing
            .author_style_sheet_parse_duration_before_fcp) {
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.CSSTiming.Parse.BeforeFirstContentfulPaint",
          timing.style_sheet_timing.author_style_sheet_parse_duration_before_fcp
              .value());
    }
    if (timing.style_sheet_timing.update_style_duration_before_fcp) {
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.CSSTiming.Update.BeforeFirstContentfulPaint",
          timing.style_sheet_timing.update_style_duration_before_fcp.value());
    }
    if (timing.style_sheet_timing
            .author_style_sheet_parse_duration_before_fcp ||
        timing.style_sheet_timing.update_style_duration_before_fcp) {
      PAGE_LOAD_HISTOGRAM(
          "PageLoad.CSSTiming.ParseAndUpdate.BeforeFirstContentfulPaint",
          timing.style_sheet_timing.author_style_sheet_parse_duration_before_fcp
                  .value_or(base::TimeDelta()) +
              timing.style_sheet_timing.update_style_duration_before_fcp
                  .value_or(base::TimeDelta()));
    }

    switch (GetPageLoadType(transition_)) {
      case LOAD_TYPE_RELOAD:
        PAGE_LOAD_HISTOGRAM(
            internal::kHistogramLoadTypeFirstContentfulPaintReload,
            timing.first_contentful_paint.value());
        // TODO(bmcquade): consider adding a histogram that uses
        // UserInputInfo.user_input_event.
        if (info.user_initiated_info.browser_initiated ||
            info.user_initiated_info.user_gesture) {
          PAGE_LOAD_HISTOGRAM(
              internal::kHistogramLoadTypeFirstContentfulPaintReloadByGesture,
              timing.first_contentful_paint.value());
        }
        break;
      case LOAD_TYPE_FORWARD_BACK:
        PAGE_LOAD_HISTOGRAM(
            internal::kHistogramLoadTypeFirstContentfulPaintForwardBack,
            timing.first_contentful_paint.value());
        if (was_no_store_main_resource_) {
          PAGE_LOAD_HISTOGRAM(
              internal::
                  kHistogramLoadTypeFirstContentfulPaintForwardBackNoStore,
              timing.first_contentful_paint.value());
        }
        break;
      case LOAD_TYPE_NEW_NAVIGATION:
        PAGE_LOAD_HISTOGRAM(
            internal::kHistogramLoadTypeFirstContentfulPaintNewNavigation,
            timing.first_contentful_paint.value());
        break;
      case LOAD_TYPE_NONE:
        NOTREACHED();
        break;
    }
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramFirstContentfulPaint,
                        timing.first_contentful_paint.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramParseStartToFirstContentfulPaint,
        timing.first_contentful_paint.value() - timing.parse_start.value());
  }
}

void CorePageLoadMetricsObserver::OnFirstMeaningfulPaint(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  base::TimeTicks paint =
      navigation_start_ + timing.first_meaningful_paint.value();
  if (first_user_interaction_after_first_paint_.is_null() ||
      paint < first_user_interaction_after_first_paint_) {
    if (WasStartedInForegroundOptionalEventInForeground(
            timing.first_meaningful_paint, info)) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstMeaningfulPaint,
          timing.first_meaningful_paint.value());
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramParseStartToFirstMeaningfulPaint,
          timing.first_meaningful_paint.value() - timing.parse_start.value());
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFirstMeaningfulPaintToNetworkStable,
          base::TimeTicks::Now() - paint);
      RecordFirstMeaningfulPaintStatus(
          internal::FIRST_MEANINGFUL_PAINT_RECORDED);
    } else {
      RecordFirstMeaningfulPaintStatus(
          internal::FIRST_MEANINGFUL_PAINT_BACKGROUNDED);
    }
  } else {
    RecordFirstMeaningfulPaintStatus(
          internal::FIRST_MEANINGFUL_PAINT_USER_INTERACTION_BEFORE_FMP);
  }
}

void CorePageLoadMetricsObserver::OnParseStart(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  if (WasStartedInForegroundOptionalEventInForeground(timing.parse_start,
                                                      info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramParseStart,
                        timing.parse_start.value());

    switch (GetPageLoadType(transition_)) {
      case LOAD_TYPE_RELOAD:
        PAGE_LOAD_HISTOGRAM(internal::kHistogramLoadTypeParseStartReload,
                            timing.parse_start.value());
        break;
      case LOAD_TYPE_FORWARD_BACK:
        PAGE_LOAD_HISTOGRAM(internal::kHistogramLoadTypeParseStartForwardBack,
                            timing.parse_start.value());
        if (was_no_store_main_resource_) {
          PAGE_LOAD_HISTOGRAM(
              internal::kHistogramLoadTypeParseStartForwardBackNoStore,
              timing.parse_start.value());
        }
        break;
      case LOAD_TYPE_NEW_NAVIGATION:
        PAGE_LOAD_HISTOGRAM(internal::kHistogramLoadTypeParseStartNewNavigation,
                            timing.parse_start.value());
        break;
      case LOAD_TYPE_NONE:
        NOTREACHED();
        break;
    }
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramParseStart,
                        timing.parse_start.value());
  }
}

void CorePageLoadMetricsObserver::OnParseStop(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  base::TimeDelta parse_duration =
      timing.parse_stop.value() - timing.parse_start.value();
  if (WasStartedInForegroundOptionalEventInForeground(timing.parse_stop,
                                                      info)) {
    PAGE_LOAD_HISTOGRAM(internal::kHistogramParseDuration, parse_duration);
    PAGE_LOAD_HISTOGRAM(internal::kHistogramParseBlockedOnScriptLoad,
                        timing.parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramParseBlockedOnScriptLoadDocumentWrite,
        timing.parse_blocked_on_script_load_from_document_write_duration
            .value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramParseBlockedOnScriptExecution,
        timing.parse_blocked_on_script_execution_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kHistogramParseBlockedOnScriptExecutionDocumentWrite,
        timing.parse_blocked_on_script_execution_from_document_write_duration
            .value());

    int total_requests = info.num_cache_requests + info.num_network_requests;
    if (total_requests) {
      int percent_cached = (100 * info.num_cache_requests) / total_requests;
      UMA_HISTOGRAM_PERCENTAGE(internal::kHistogramCacheRequestPercentParseStop,
                               percent_cached);
      UMA_HISTOGRAM_COUNTS(internal::kHistogramCacheTotalRequestsParseStop,
                           info.num_cache_requests);
      UMA_HISTOGRAM_COUNTS(internal::kHistogramTotalRequestsParseStop,
                           info.num_cache_requests + info.num_network_requests);

      // Separate out parse duration based on cache percent.
      if (percent_cached <= 50) {
        PAGE_LOAD_HISTOGRAM(
            "PageLoad.Experimental.ParseDuration.CachedPercent.0-50",
            parse_duration);
      } else {
        PAGE_LOAD_HISTOGRAM(
            "PageLoad.Experimental.ParseDuration.CachedPercent.51-100",
            parse_duration);
      }
    }
  } else {
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramParseDuration,
                        parse_duration);
    PAGE_LOAD_HISTOGRAM(internal::kBackgroundHistogramParseBlockedOnScriptLoad,
                        timing.parse_blocked_on_script_load_duration.value());
    PAGE_LOAD_HISTOGRAM(
        internal::kBackgroundHistogramParseBlockedOnScriptLoadDocumentWrite,
        timing.parse_blocked_on_script_load_from_document_write_duration
            .value());
  }
}

void CorePageLoadMetricsObserver::OnComplete(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  RecordTimingHistograms(timing, info);
  RecordRappor(timing, info);

  int64_t total_kb = (info.network_bytes + info.cache_bytes) / 1024;
  int64_t network_kb = info.network_bytes / 1024;
  int64_t cache_kb = info.cache_bytes / 1024;
  DCHECK_LE(network_kb, total_kb);
  DCHECK_LE(cache_kb, total_kb);
  DCHECK_LE(total_kb, std::numeric_limits<int>::max());

  UMA_HISTOGRAM_CUSTOM_COUNTS(internal::kHistogramNetworkBytes,
                              static_cast<int>(network_kb), 1, 500 * 1024, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS(internal::kHistogramCacheBytes,
                              static_cast<int>(cache_kb), 1, 500 * 1024, 50);
  UMA_HISTOGRAM_CUSTOM_COUNTS(internal::kHistogramTotalBytes,
                              static_cast<int>(total_kb), 1, 500 * 1024, 50);
}

void CorePageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo& failed_load_info,
    const page_load_metrics::PageLoadExtraInfo& extra_info) {
  // Only handle actual failures; provisional loads that failed due to another
  // committed load or due to user action are recorded in
  // AbortsPageLoadMetricsObserver.
  if (failed_load_info.error != net::OK &&
      failed_load_info.error != net::ERR_ABORTED) {
    if (WasStartedInForegroundOptionalEventInForeground(
            failed_load_info.time_to_failed_provisional_load, extra_info)) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFailedProvisionalLoad,
                          failed_load_info.time_to_failed_provisional_load);
    }
  }
}

void CorePageLoadMetricsObserver::OnUserInput(
    const blink::WebInputEvent& event) {
  base::TimeTicks now;
  if (!first_paint_.is_null() &&
      first_user_interaction_after_first_paint_.is_null() &&
      event.type != blink::WebInputEvent::MouseMove) {
    if (now.is_null())
      now = base::TimeTicks::Now();
    first_user_interaction_after_first_paint_ = now;
  }

  if (first_paint_.is_null())
    return;

  if (!received_non_scroll_input_after_first_paint_) {
    if (event.type == blink::WebInputEvent::GestureTap ||
        event.type == blink::WebInputEvent::MouseUp) {
      received_non_scroll_input_after_first_paint_ = true;
      if (now.is_null())
        now = base::TimeTicks::Now();
      PAGE_LOAD_HISTOGRAM(
          internal::kHistogramFirstNonScrollInputAfterFirstPaint,
          now - first_paint_);
    }
  }
  if (!received_scroll_input_after_first_paint_ &&
      event.type == blink::WebInputEvent::GestureScrollBegin) {
    received_scroll_input_after_first_paint_ = true;
    if (now.is_null())
      now = base::TimeTicks::Now();
    PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstScrollInputAfterFirstPaint,
                        now - first_paint_);
  }
}

void CorePageLoadMetricsObserver::RecordTimingHistograms(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  // Log time to first foreground / time to first background. Log counts that we
  // started a relevant page load in the foreground / background.
  if (!info.started_in_foreground) {
    if (info.first_foreground_time)
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstForeground,
                          info.first_foreground_time.value());
  }

  if (timing.first_paint && !timing.first_meaningful_paint) {
    RecordFirstMeaningfulPaintStatus(
        timing.first_contentful_paint ?
        internal::FIRST_MEANINGFUL_PAINT_DID_NOT_REACH_NETWORK_STABLE :
        internal::FIRST_MEANINGFUL_PAINT_DID_NOT_REACH_FIRST_CONTENTFUL_PAINT);
  }

  if (timing.first_paint) {
    enum FirstMeaningfulPaintSignalStatus {
      HAD_USER_INPUT = 1 << 0,
      NETWORK_STABLE = 1 << 1,
      FIRST_MEANINGFUL_PAINT_SIGNAL_STATUS_LAST_ENTRY = 1 << 2
    };
    int signal_status =
        (first_user_interaction_after_first_paint_.is_null() ?
         0 : HAD_USER_INPUT) +
        (timing.first_meaningful_paint ? NETWORK_STABLE : 0);
    UMA_HISTOGRAM_ENUMERATION(
        internal::kHistogramFirstMeaningfulPaintSignalStatus2,
        signal_status, FIRST_MEANINGFUL_PAINT_SIGNAL_STATUS_LAST_ENTRY);
  }
  if (timing.first_meaningful_paint) {
    if (first_user_interaction_after_first_paint_.is_null()) {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstMeaningfulPaintNoUserInput,
                          timing.first_meaningful_paint.value());
    } else {
      PAGE_LOAD_HISTOGRAM(internal::kHistogramFirstMeaningfulPaintHadUserInput,
                          timing.first_meaningful_paint.value());
    }
  }
}

void CorePageLoadMetricsObserver::RecordRappor(
    const page_load_metrics::PageLoadTiming& timing,
    const page_load_metrics::PageLoadExtraInfo& info) {
  // During the browser process shutdown path, calling
  // BrowserProcess::rappor_service() can reinitialize multiple destroyed
  // objects. This alters shutdown ordering, so we avoid it by testing
  // IsShuttingDown() first.
  if (g_browser_process->IsShuttingDown())
    return;
  rappor::RapporServiceImpl* rappor_service =
      g_browser_process->rappor_service();
  if (!rappor_service)
    return;
  if (info.committed_url.is_empty())
    return;

  // Log the eTLD+1 of sites that show poor loading performance.
  if (WasStartedInForegroundOptionalEventInForeground(
          timing.first_contentful_paint, info)) {
    std::unique_ptr<rappor::Sample> sample =
        rappor_service->CreateSample(rappor::UMA_RAPPOR_TYPE);
    sample->SetStringField(
        "Domain",
        rappor::GetDomainAndRegistrySampleFromGURL(info.committed_url));
    uint64_t bucket_index =
        RapporHistogramBucketIndex(timing.first_contentful_paint.value());
    sample->SetFlagsField("Bucket", uint64_t(1) << bucket_index,
                          kNumRapporHistogramBuckets);
    // The IsSlow flag is just a one bit boolean if the first contentful paint
    // was > 10s.
    sample->SetFlagsField(
        "IsSlow", timing.first_contentful_paint.value().InSecondsF() >= 10, 1);
    rappor_service->RecordSample(internal::kRapporMetricsNameCoarseTiming,
                                 std::move(sample));
  }

  // Log the eTLD+1 of sites that did not report first meaningful paint.
  if (timing.first_paint && !timing.first_meaningful_paint) {
    rappor::SampleDomainAndRegistryFromGURL(
        rappor_service,
        internal::kRapporMetricsNameFirstMeaningfulPaintNotRecorded,
        info.committed_url);
  }
}
