// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/physical_web_provider.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/bookmarks/browser/titled_url_index.h"
#include "components/bookmarks/browser/titled_url_node_sorter.h"
#include "components/omnibox/browser/autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_provider_listener.h"
#include "components/omnibox/browser/history_url_provider.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/physical_web_node.h"
#include "components/omnibox/browser/titled_url_match_utils.h"
#include "components/omnibox/browser/url_prefix.h"
#include "components/omnibox/browser/verbatim_match.h"
#include "components/physical_web/data_source/physical_web_data_source.h"
#include "components/url_formatter/url_formatter.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"
#include "url/gurl.h"

namespace {

// The maximum length of the page title's part of the overflow item's
// description. Longer titles will be truncated to this length. In a normal
// Physical Web match item (non-overflow item) we allow the omnibox display to
// truncate the title instead.
static const size_t kMaxTitleLengthInOverflow = 15;

// The maximum number of Physical Web URLs to retrieve from the index.
static const size_t kPhysicalWebIndexMaxMatches = 50;
}

// static
const size_t PhysicalWebProvider::kPhysicalWebMaxMatches = 1;

// static
PhysicalWebProvider* PhysicalWebProvider::Create(
    AutocompleteProviderClient* client,
    HistoryURLProvider* history_url_provider) {
  return new PhysicalWebProvider(client, history_url_provider);
}

void PhysicalWebProvider::Start(const AutocompleteInput& input,
                                bool minimal_changes) {
  DCHECK(kPhysicalWebMaxMatches < kMaxMatches);

  Stop(false, false);

  done_ = false;
  matches_.clear();

  had_physical_web_suggestions_ = false;
  if (input.from_omnibox_focus())
    had_physical_web_suggestions_at_focus_or_later_ = false;

  // Don't provide suggestions in incognito mode.
  if (client_->IsOffTheRecord()) {
    done_ = true;
    nearby_url_count_ = 0;
    return;
  }

  physical_web::PhysicalWebDataSource* data_source =
      client_->GetPhysicalWebDataSource();
  if (!data_source) {
    done_ = true;
    nearby_url_count_ = 0;
    return;
  }

  if (input.from_omnibox_focus()) {
    ConstructZeroSuggestMatches(data_source->GetMetadata());

    if (!matches_.empty()) {
      had_physical_web_suggestions_ = true;
      had_physical_web_suggestions_at_focus_or_later_ = true;
    }

    if (!zero_suggest_enabled_) {
      matches_.clear();
    }

    // In zero-suggest, Physical Web matches should never be default. If the
    // omnibox input is non-empty and we have at least one Physical Web match,
    // add the current URL as the default so that hitting enter after focusing
    // the omnibox causes the current page to reload. If the input field is
    // empty, no default match is required.
    if (!matches_.empty() && !input.text().empty()) {
      matches_.push_back(VerbatimMatchForURL(
          client_, input, input.current_url(), history_url_provider_, -1));
    }
  } else {
    ConstructQuerySuggestMatches(data_source->GetMetadata(), input);

    if (!matches_.empty()) {
      had_physical_web_suggestions_ = true;
      had_physical_web_suggestions_at_focus_or_later_ = true;
    }

    if (!after_typing_enabled_) {
      matches_.clear();
    }
  }

  done_ = true;
}

void PhysicalWebProvider::Stop(bool clear_cached_results,
                               bool due_to_user_inactivity) {
  done_ = true;
}

void PhysicalWebProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
  // Record whether the provider could have provided a Physical Web suggestion,
  // even if the suggestion could not be displayed due to the current field
  // trial.
  provider_info->push_back(metrics::OmniboxEventProto_ProviderInfo());
  metrics::OmniboxEventProto_ProviderInfo& new_entry = provider_info->back();
  new_entry.set_provider(AsOmniboxEventProviderType());
  new_entry.set_provider_done(done_);
  std::vector<uint32_t> field_trial_hashes;
  OmniboxFieldTrial::GetActiveSuggestFieldTrialHashes(&field_trial_hashes);
  for (size_t i = 0; i < field_trial_hashes.size(); ++i) {
    if (had_physical_web_suggestions_)
      new_entry.mutable_field_trial_triggered()->Add(field_trial_hashes[i]);
    if (had_physical_web_suggestions_at_focus_or_later_) {
      new_entry.mutable_field_trial_triggered_in_session()->Add(
          field_trial_hashes[i]);
    }
  }

  // When the user accepts an autocomplete suggestion, record the number of
  // nearby Physical Web URLs at the time the provider last constructed matches.
  UMA_HISTOGRAM_EXACT_LINEAR("Omnibox.SuggestionUsed.NearbyURLCount",
                             nearby_url_count_, 50);
}

PhysicalWebProvider::PhysicalWebProvider(
    AutocompleteProviderClient* client,
    HistoryURLProvider* history_url_provider)
    : AutocompleteProvider(AutocompleteProvider::TYPE_PHYSICAL_WEB),
      client_(client),
      history_url_provider_(history_url_provider),
      zero_suggest_enabled_(
          OmniboxFieldTrial::InPhysicalWebZeroSuggestFieldTrial()),
      after_typing_enabled_(
          OmniboxFieldTrial::InPhysicalWebAfterTypingFieldTrial()),
      zero_suggest_base_relevance_(
          OmniboxFieldTrial::GetPhysicalWebZeroSuggestBaseRelevance()),
      after_typing_base_relevance_(
          OmniboxFieldTrial::GetPhysicalWebAfterTypingBaseRelevance()) {}

PhysicalWebProvider::~PhysicalWebProvider() {
}

void PhysicalWebProvider::ConstructZeroSuggestMatches(
    std::unique_ptr<base::ListValue> metadata_list) {
  nearby_url_count_ = metadata_list->GetSize();
  size_t used_slots = 0;

  for (size_t i = 0; i < nearby_url_count_; ++i) {
    base::DictionaryValue* metadata_item = NULL;
    if (!metadata_list->GetDictionary(i, &metadata_item)) {
      continue;
    }

    std::string url_string;
    std::string title_string;
    if (!metadata_item->GetString(physical_web::kResolvedUrlKey, &url_string) ||
        !metadata_item->GetString(physical_web::kTitleKey, &title_string)) {
      continue;
    }
    base::string16 title =
        AutocompleteMatch::SanitizeString(base::UTF8ToUTF16(title_string));

    // Add match items with decreasing relevance to preserve the ordering in
    // the metadata list.
    int relevance = zero_suggest_base_relevance_ - used_slots;

    // Append an overflow item if creating a match for each metadata item would
    // exceed the match limit.
    const size_t remaining_slots = kPhysicalWebMaxMatches - used_slots;
    const size_t remaining_metadata = nearby_url_count_ - i;
    if ((remaining_slots == 1) && (remaining_metadata > remaining_slots)) {
      AppendOverflowItem(remaining_metadata, relevance, title);
      break;
    }

    GURL url(url_string);

    AutocompleteMatch match(this, relevance, false,
        AutocompleteMatchType::PHYSICAL_WEB);
    match.destination_url = url;

    // Physical Web results should omit http:// (but not https://) and never
    // appear bold.
    match.contents = url_formatter::FormatUrl(url,
        url_formatter::kFormatUrlOmitHTTP, net::UnescapeRule::SPACES,
        nullptr, nullptr, nullptr);
    match.contents_class.push_back(
        ACMatchClassification(0, ACMatchClassification::URL));

    match.fill_into_edit =
        AutocompleteInput::FormattedStringWithEquivalentMeaning(
            url, match.contents, client_->GetSchemeClassifier());

    match.description = title;
    match.description_class.push_back(
        ACMatchClassification(0, ACMatchClassification::NONE));

    matches_.push_back(match);
    ++used_slots;
  }

  UMA_HISTOGRAM_EXACT_LINEAR(
      "Omnibox.PhysicalWebProviderMatches", matches_.size(), kMaxMatches);
}

void PhysicalWebProvider::ConstructQuerySuggestMatches(
    std::unique_ptr<base::ListValue> metadata_list,
    const AutocompleteInput& input) {
  // Passing nullptr for the TitledUrlNodeSorter will cause the returned match
  // list to be unsorted.
  bookmarks::TitledUrlIndex index(nullptr);
  std::vector<std::unique_ptr<PhysicalWebNode>> nodes;
  const size_t metadata_count = metadata_list->GetSize();
  for (size_t i = 0; i < metadata_count; ++i) {
    base::DictionaryValue* metadata_item = NULL;
    if (metadata_list->GetDictionary(i, &metadata_item)) {
      nodes.push_back(base::MakeUnique<PhysicalWebNode>(*metadata_item));
      index.Add(nodes.back().get());
    }
  }

  std::vector<bookmarks::TitledUrlMatch> titled_url_matches;
  index.GetResultsMatching(input.text(), kPhysicalWebIndexMaxMatches,
                           query_parser::MatchingAlgorithm::DEFAULT,
                           &titled_url_matches);

  size_t used_slots = 0;
  const base::string16 fixed_up_input(FixupUserInput(input).second);
  for (auto titled_url_match : titled_url_matches) {
    const int relevance = after_typing_base_relevance_ - used_slots;
    matches_.push_back(bookmarks::TitledUrlMatchToAutocompleteMatch(
        titled_url_match, AutocompleteMatchType::PHYSICAL_WEB, relevance, this,
        client_->GetSchemeClassifier(), input, fixed_up_input));
    ++used_slots;
    if (matches_.size() >= kPhysicalWebMaxMatches) {
      break;
    }
  }
}

void PhysicalWebProvider::AppendOverflowItem(int additional_url_count,
                                             int relevance,
                                             const base::string16& title) {
  std::string url_string = "chrome://physical-web";
  GURL url(url_string);

  AutocompleteMatch match(this, relevance, false,
      AutocompleteMatchType::PHYSICAL_WEB_OVERFLOW);
  match.destination_url = url;

  base::string16 contents_title = gfx::TruncateString(
      title, kMaxTitleLengthInOverflow, gfx::CHARACTER_BREAK);
  if (contents_title.empty()) {
    match.contents = l10n_util::GetPluralStringFUTF16(
        IDS_PHYSICAL_WEB_OVERFLOW_EMPTY_TITLE, additional_url_count);
  } else {
    base::string16 contents_suffix = l10n_util::GetPluralStringFUTF16(
        IDS_PHYSICAL_WEB_OVERFLOW, additional_url_count - 1);
    match.contents = contents_title + base::UTF8ToUTF16(" ") + contents_suffix;
  }
  match.contents_class.push_back(
      ACMatchClassification(0, ACMatchClassification::DIM));

  match.fill_into_edit =
      AutocompleteInput::FormattedStringWithEquivalentMeaning(
          url, match.contents, client_->GetSchemeClassifier());

  match.description =
      l10n_util::GetStringUTF16(IDS_PHYSICAL_WEB_OVERFLOW_DESCRIPTION);
  match.description_class.push_back(
      ACMatchClassification(0, ACMatchClassification::NONE));

  matches_.push_back(match);
}
