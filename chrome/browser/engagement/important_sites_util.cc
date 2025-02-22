// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/engagement/important_sites_util.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/containers/hash_tables.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/engagement/site_engagement_score.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/WebKit/public/platform/site_engagement.mojom.h"
#include "url/gurl.h"

namespace {
using bookmarks::BookmarkModel;
using ImportantDomainInfo = ImportantSitesUtil::ImportantDomainInfo;

static const char kNumTimesIgnoredName[] = "NumTimesIgnored";
static const int kTimesIgnoredForBlacklist = 3;

// These are the maximum # of bookmarks we can use as signals. If the user has
// <= kMaxBookmarks, then we just use those bookmarks. Otherwise we filter all
// bookmarks on site engagement > 0, sort, and trim to kMaxBookmarks.
static const int kMaxBookmarks = 5;

// Do not change the values here, as they are used for UMA histograms.
enum ImportantReason {
  ENGAGEMENT = 0,
  DURABLE = 1,
  BOOKMARKS = 2,
  HOME_SCREEN = 3,
  NOTIFICATIONS = 4,
  REASON_BOUNDARY
};

// We need this to be a macro, as the histogram macros cache their pointers
// after the first call, so when we change the uma name we check fail if we're
// just a method.
#define RECORD_UMA_FOR_IMPORTANT_REASON(uma_name, uma_count_name,    \
                                        reason_bitfield)             \
  do {                                                               \
    int count = 0;                                                   \
    int32_t bitfield = (reason_bitfield);                            \
    for (int i = 0; i < ImportantReason::REASON_BOUNDARY; i++) {     \
      if ((bitfield >> i) & 1) {                                     \
        count++;                                                     \
        UMA_HISTOGRAM_ENUMERATION((uma_name), i,                     \
                                  ImportantReason::REASON_BOUNDARY); \
      }                                                              \
    }                                                                \
    UMA_HISTOGRAM_ENUMERATION((uma_count_name), count,               \
                              ImportantReason::REASON_BOUNDARY);     \
  } while (0)

// Do not change the values here, as they are used for UMA histograms and
// testing in important_sites_util_unittest.
enum CrossedReason {
  CROSSED_DURABLE = 0,
  CROSSED_NOTIFICATIONS = 1,
  CROSSED_ENGAGEMENT = 2,
  CROSSED_NOTIFICATIONS_AND_ENGAGEMENT = 3,
  CROSSED_DURABLE_AND_ENGAGEMENT = 4,
  CROSSED_NOTIFICATIONS_AND_DURABLE = 5,
  CROSSED_NOTIFICATIONS_AND_DURABLE_AND_ENGAGEMENT = 6,
  CROSSED_REASON_UNKNOWN = 7,
  CROSSED_REASON_BOUNDARY
};

CrossedReason GetCrossedReasonFromBitfield(int32_t reason_bitfield) {
  bool durable = (reason_bitfield & (1 << ImportantReason::DURABLE)) != 0;
  bool notifications =
      (reason_bitfield & (1 << ImportantReason::NOTIFICATIONS)) != 0;
  bool engagement = (reason_bitfield & (1 << ImportantReason::ENGAGEMENT)) != 0;
  if (durable && notifications && engagement)
    return CROSSED_NOTIFICATIONS_AND_DURABLE_AND_ENGAGEMENT;
  else if (notifications && durable)
    return CROSSED_NOTIFICATIONS_AND_DURABLE;
  else if (notifications && engagement)
    return CROSSED_NOTIFICATIONS_AND_ENGAGEMENT;
  else if (durable && engagement)
    return CROSSED_DURABLE_AND_ENGAGEMENT;
  else if (notifications)
    return CROSSED_NOTIFICATIONS;
  else if (durable)
    return CROSSED_DURABLE;
  else if (engagement)
    return CROSSED_ENGAGEMENT;
  return CROSSED_REASON_UNKNOWN;
}

std::string GetRegisterableDomainOrIP(const GURL& url) {
  std::string registerable_domain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  if (registerable_domain.empty() && url.HostIsIPAddress())
    registerable_domain = url.host();
  return registerable_domain;
}

void MaybePopulateImportantInfoForReason(
    const GURL& origin,
    std::set<GURL>* visited_origins,
    ImportantReason reason,
    base::hash_map<std::string, ImportantDomainInfo>* output) {
  if (!origin.is_valid() || !visited_origins->insert(origin).second)
    return;
  std::string registerable_domain = GetRegisterableDomainOrIP(origin);
  ImportantDomainInfo& info = (*output)[registerable_domain];
  info.reason_bitfield |= 1 << reason;
  if (info.example_origin.is_empty()) {
    info.registerable_domain = registerable_domain;
    info.example_origin = origin;
  }
}

// Returns the score associated with the given reason. The order of
// ImportantReason does not need to correspond to the score order. The higher
// the score, the more important the reason is.
int GetScoreForReason(ImportantReason reason) {
  switch (reason) {
    case ImportantReason::ENGAGEMENT:
      return 1 << 0;
    case ImportantReason::DURABLE:
      return 1 << 1;
    case ImportantReason::BOOKMARKS:
      return 1 << 2;
    case ImportantReason::HOME_SCREEN:
      return 1 << 3;
    case ImportantReason::NOTIFICATIONS:
      return 1 << 4;
    case ImportantReason::REASON_BOUNDARY:
      return 0;
  }
  return 0;
}

int GetScoreForReasonsBitfield(int32_t reason_bitfield) {
  int score = 0;
  for (int i = 0; i < ImportantReason::REASON_BOUNDARY; i++) {
    if ((reason_bitfield >> i) & 1) {
      score += GetScoreForReason(static_cast<ImportantReason>(i));
    }
  }
  return score;
}

// Returns if |a| has a higher score than |b|, so that when we sort the higher
// score is first.
bool CompareDescendingImportantInfo(
    const std::pair<std::string, ImportantDomainInfo>& a,
    const std::pair<std::string, ImportantDomainInfo>& b) {
  int score_a = GetScoreForReasonsBitfield(a.second.reason_bitfield);
  int score_b = GetScoreForReasonsBitfield(b.second.reason_bitfield);
  int bitfield_diff = score_a - score_b;
  if (bitfield_diff != 0)
    return bitfield_diff > 0;
  return a.second.engagement_score > b.second.engagement_score;
}

base::hash_set<std::string> GetBlacklistedImportantDomains(Profile* profile) {
  ContentSettingsForOneType content_settings_list;
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  map->GetSettingsForOneType(CONTENT_SETTINGS_TYPE_IMPORTANT_SITE_INFO,
                             content_settings::ResourceIdentifier(),
                             &content_settings_list);
  base::hash_set<std::string> ignoring_domains;
  for (const ContentSettingPatternSource& site : content_settings_list) {
    GURL origin(site.primary_pattern.ToString());
    if (!origin.is_valid() ||
        base::ContainsKey(ignoring_domains, origin.host())) {
      continue;
    }

    std::unique_ptr<base::DictionaryValue> dict =
        base::DictionaryValue::From(map->GetWebsiteSetting(
            origin, origin, CONTENT_SETTINGS_TYPE_IMPORTANT_SITE_INFO, "",
            nullptr));

    if (!dict)
      continue;

    int times_ignored = 0;
    if (!dict->GetInteger(kNumTimesIgnoredName, &times_ignored) ||
        times_ignored < kTimesIgnoredForBlacklist) {
      continue;
    }

    ignoring_domains.insert(origin.host());
  }
  return ignoring_domains;
}

void PopulateInfoMapWithSiteEngagement(
    Profile* profile,
    blink::mojom::EngagementLevel minimum_engagement,
    std::map<GURL, double>* engagement_map,
    base::hash_map<std::string, ImportantDomainInfo>* output) {
  SiteEngagementService* service = SiteEngagementService::Get(profile);
  *engagement_map = service->GetScoreMap();
  // We can have multiple origins for a single domain, so we record the one
  // with the highest engagement score.
  for (const auto& url_engagement_pair : *engagement_map) {
    if (!service->IsEngagementAtLeast(url_engagement_pair.first,
                                      minimum_engagement)) {
      continue;
    }
    std::string registerable_domain =
        GetRegisterableDomainOrIP(url_engagement_pair.first);
    ImportantDomainInfo& info = (*output)[registerable_domain];
    if (url_engagement_pair.second > info.engagement_score) {
      info.registerable_domain = registerable_domain;
      info.engagement_score = url_engagement_pair.second;
      info.example_origin = url_engagement_pair.first;
      info.reason_bitfield |= 1 << ImportantReason::ENGAGEMENT;
    }
  }
}

void PopulateInfoMapWithContentTypeAllowed(
    Profile* profile,
    ContentSettingsType content_type,
    ImportantReason reason,
    base::hash_map<std::string, ImportantDomainInfo>* output) {
  // Grab our content settings list.
  ContentSettingsForOneType content_settings_list;
  HostContentSettingsMapFactory::GetForProfile(profile)->GetSettingsForOneType(
      content_type, content_settings::ResourceIdentifier(),
      &content_settings_list);
  // Extract a set of urls, using the primary pattern. We don't handle
  // wildcard patterns.
  std::set<GURL> content_origins;
  for (const ContentSettingPatternSource& site : content_settings_list) {
    if (site.setting != CONTENT_SETTING_ALLOW)
      continue;
    MaybePopulateImportantInfoForReason(GURL(site.primary_pattern.ToString()),
                                        &content_origins, reason, output);
  }
}

void PopulateInfoMapWithBookmarks(
    Profile* profile,
    const std::map<GURL, double>& engagement_map,
    base::hash_map<std::string, ImportantDomainInfo>* output) {
  SiteEngagementService* service = SiteEngagementService::Get(profile);
  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContextIfExists(profile);
  if (!model)
    return;
  std::vector<BookmarkModel::URLAndTitle> untrimmed_bookmarks;
  model->GetBookmarks(&untrimmed_bookmarks);

  // Process the bookmarks and optionally trim them if we have too many.
  std::vector<BookmarkModel::URLAndTitle> result_bookmarks;
  if (untrimmed_bookmarks.size() > kMaxBookmarks) {
    std::copy_if(untrimmed_bookmarks.begin(), untrimmed_bookmarks.end(),
                 std::back_inserter(result_bookmarks),
                 [service](const BookmarkModel::URLAndTitle& entry) {
                   return service->IsEngagementAtLeast(
                       entry.url.GetOrigin(),
                       blink::mojom::EngagementLevel::LOW);
                 });
    std::sort(result_bookmarks.begin(), result_bookmarks.end(),
              [&engagement_map](const BookmarkModel::URLAndTitle& a,
                                const BookmarkModel::URLAndTitle& b) {
                double a_score = engagement_map.at(a.url.GetOrigin());
                double b_score = engagement_map.at(b.url.GetOrigin());
                return a_score > b_score;
              });
    if (result_bookmarks.size() > kMaxBookmarks)
      result_bookmarks.resize(kMaxBookmarks);
  } else {
    result_bookmarks = std::move(untrimmed_bookmarks);
  }

  std::set<GURL> content_origins;
  for (const BookmarkModel::URLAndTitle& bookmark : result_bookmarks) {
    MaybePopulateImportantInfoForReason(bookmark.url, &content_origins,
                                        ImportantReason::BOOKMARKS, output);
  }
}

void PopulateInfoMapWithHomeScreen(
    Profile* profile,
    base::hash_map<std::string, ImportantDomainInfo>* output) {
  ContentSettingsForOneType content_settings_list;
  HostContentSettingsMapFactory::GetForProfile(profile)->GetSettingsForOneType(
      CONTENT_SETTINGS_TYPE_APP_BANNER, content_settings::ResourceIdentifier(),
      &content_settings_list);
  // Extract a set of urls, using the primary pattern. We don't handle
  // wildcard patterns.
  std::set<GURL> content_origins;
  base::Time now = base::Time::Now();
  for (const ContentSettingPatternSource& site : content_settings_list) {
    GURL origin(site.primary_pattern.ToString());
    if (!AppBannerSettingsHelper::WasLaunchedRecently(profile, origin, now))
      continue;
    MaybePopulateImportantInfoForReason(origin, &content_origins,
                                        ImportantReason::HOME_SCREEN, output);
  }
}

}  // namespace

std::vector<ImportantDomainInfo>
ImportantSitesUtil::GetImportantRegisterableDomains(Profile* profile,
                                                    size_t max_results) {
  base::hash_map<std::string, ImportantDomainInfo> important_info;
  std::map<GURL, double> engagement_map;

  PopulateInfoMapWithSiteEngagement(
      profile, blink::mojom::EngagementLevel::MEDIUM, &engagement_map,
      &important_info);

  PopulateInfoMapWithContentTypeAllowed(
      profile, CONTENT_SETTINGS_TYPE_NOTIFICATIONS,
      ImportantReason::NOTIFICATIONS, &important_info);

  PopulateInfoMapWithContentTypeAllowed(
      profile, CONTENT_SETTINGS_TYPE_DURABLE_STORAGE, ImportantReason::DURABLE,
      &important_info);

  PopulateInfoMapWithBookmarks(profile, engagement_map, &important_info);

  PopulateInfoMapWithHomeScreen(profile, &important_info);

  base::hash_set<std::string> blacklisted_domains =
      GetBlacklistedImportantDomains(profile);

  std::vector<std::pair<std::string, ImportantDomainInfo>> items(
      important_info.begin(), important_info.end());
  std::sort(items.begin(), items.end(), &CompareDescendingImportantInfo);

  std::vector<ImportantDomainInfo> final_list;
  for (std::pair<std::string, ImportantDomainInfo>& domain_info : items) {
    if (final_list.size() >= max_results)
      return final_list;
    if (blacklisted_domains.find(domain_info.first) !=
        blacklisted_domains.end()) {
      continue;
    }
    final_list.push_back(domain_info.second);
    RECORD_UMA_FOR_IMPORTANT_REASON(
        "Storage.ImportantSites.GeneratedReason",
        "Storage.ImportantSites.GeneratedReasonCount",
        domain_info.second.reason_bitfield);
  }

  return final_list;
}

void ImportantSitesUtil::RecordBlacklistedAndIgnoredImportantSites(
    Profile* profile,
    const std::vector<std::string>& blacklisted_sites,
    const std::vector<int32_t>& blacklisted_sites_reason_bitfield,
    const std::vector<std::string>& ignored_sites,
    const std::vector<int32_t>& ignored_sites_reason_bitfield) {
  // First, record the metrics for blacklisted and ignored sites.
  for (int32_t reason_bitfield : blacklisted_sites_reason_bitfield) {
    RECORD_UMA_FOR_IMPORTANT_REASON(
        "Storage.ImportantSites.CBDChosenReason",
        "Storage.ImportantSites.CBDChosenReasonCount", reason_bitfield);
  }
  for (int32_t reason_bitfield : ignored_sites_reason_bitfield) {
    RECORD_UMA_FOR_IMPORTANT_REASON(
        "Storage.ImportantSites.CBDIgnoredReason",
        "Storage.ImportantSites.CBDIgnoredReasonCount", reason_bitfield);
  }

  // We use the ignored sites to update our important sites blacklist.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  for (const std::string& ignored_site : ignored_sites) {
    GURL origin("http://" + ignored_site);
    std::unique_ptr<base::Value> value = map->GetWebsiteSetting(
        origin, origin, CONTENT_SETTINGS_TYPE_IMPORTANT_SITE_INFO, "", nullptr);

    std::unique_ptr<base::DictionaryValue> dict =
        base::DictionaryValue::From(map->GetWebsiteSetting(
            origin, origin, CONTENT_SETTINGS_TYPE_IMPORTANT_SITE_INFO, "",
            nullptr));

    int times_ignored = 0;
    if (dict)
      dict->GetInteger(kNumTimesIgnoredName, &times_ignored);
    else
      dict = base::MakeUnique<base::DictionaryValue>();
    dict->SetInteger(kNumTimesIgnoredName, ++times_ignored);

    map->SetWebsiteSettingDefaultScope(
        origin, origin, CONTENT_SETTINGS_TYPE_IMPORTANT_SITE_INFO, "",
        std::move(dict));
  }

  // We clear our blacklist for sites that the user chose.
  for (const std::string& ignored_site : blacklisted_sites) {
    GURL origin("http://" + ignored_site);
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue());
    dict->SetInteger(kNumTimesIgnoredName, 0);
    map->SetWebsiteSettingDefaultScope(
        origin, origin, CONTENT_SETTINGS_TYPE_IMPORTANT_SITE_INFO, "",
        std::move(dict));
  }

  // Finally, record our old crossed-stats.
  // Note: we don't plan on adding new metrics here, this is just for the finch
  // experiment to give us initial data on what signals actually mattered.
  for (int32_t reason_bitfield : blacklisted_sites_reason_bitfield) {
    UMA_HISTOGRAM_ENUMERATION("Storage.BlacklistedImportantSites.Reason",
                              GetCrossedReasonFromBitfield(reason_bitfield),
                              CROSSED_REASON_BOUNDARY);
  }
}

void ImportantSitesUtil::MarkOriginAsImportantForTesting(Profile* profile,
                                                         const GURL& origin) {
  SiteEngagementScore::SetParamValuesForTesting();
  // First get data from site engagement.
  SiteEngagementService* site_engagement_service =
      SiteEngagementService::Get(profile);
  site_engagement_service->ResetScoreForURL(
      origin, SiteEngagementScore::GetMediumEngagementBoundary());
  DCHECK(site_engagement_service->IsEngagementAtLeast(
      origin, blink::mojom::EngagementLevel::MEDIUM));
}
