// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/contextual_search_delegate.h"

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/browser/android/contextualsearch/contextual_search_field_trial.h"
#include "chrome/browser/android/contextualsearch/resolved_search_term.h"
#include "chrome/browser/android/proto/client_discourse_context.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "net/http/http_status_code.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

using content::ContentViewCore;
using content::RenderFrameHost;

namespace {

const char kContextualSearchResponseDisplayTextParam[] = "display_text";
const char kContextualSearchResponseSelectedTextParam[] = "selected_text";
const char kContextualSearchResponseSearchTermParam[] = "search_term";
const char kContextualSearchResponseLanguageParam[] = "lang";
const char kContextualSearchResponseMidParam[] = "mid";
const char kContextualSearchResponseResolvedTermParam[] = "resolved_term";
const char kContextualSearchPreventPreload[] = "prevent_preload";
const char kContextualSearchMentions[] = "mentions";
const char kContextualSearchCaption[] = "caption";
const char kContextualSearchThumbnail[] = "thumbnail";
const char kContextualSearchAction[] = "action";
const char kContextualSearchCategory[] = "category";

const char kActionCategoryAddress[] = "ADDRESS";
const char kActionCategoryEmail[] = "EMAIL";
const char kActionCategoryEvent[] = "EVENT";
const char kActionCategoryPhone[] = "PHONE";

const char kContextualSearchServerEndpoint[] = "_/contextualsearch?";
const int kContextualSearchRequestVersion = 2;
const int kContextualSearchMaxSelection = 100;
const char kXssiEscape[] = ")]}'\n";
const char kDiscourseContextHeaderPrefix[] = "X-Additional-Discourse-Context: ";
const char kDoPreventPreloadValue[] = "1";

// The number of characters that should be shown after the selected expression.
const int kSurroundingSizeForUI = 60;

// The version of the Contextual Cards API that we want to invoke.
const int kContextualCardsNoIntegration = 0;
const int kContextualCardsBarIntegration = 1;
const int kContextualCardsSingleAction = 2;

const char kContextualCardsVersionOverride[] = "contextual_cards_version";

}  // namespace

// URLFetcher ID, only used for tests: we only have one kind of fetcher.
const int ContextualSearchDelegate::kContextualSearchURLFetcherID = 1;

// Handles tasks for the ContextualSearchManager in a separable, testable way.
ContextualSearchDelegate::ContextualSearchDelegate(
    net::URLRequestContextGetter* url_request_context,
    TemplateURLService* template_url_service,
    const ContextualSearchDelegate::SearchTermResolutionCallback&
        search_term_callback,
    const ContextualSearchDelegate::SurroundingTextCallback&
        surrounding_callback,
    const ContextualSearchDelegate::IcingCallback& icing_callback)
    : url_request_context_(url_request_context),
      template_url_service_(template_url_service),
      search_term_callback_(search_term_callback),
      surrounding_callback_(surrounding_callback),
      icing_callback_(icing_callback) {
  field_trial_.reset(new ContextualSearchFieldTrial());
}

ContextualSearchDelegate::~ContextualSearchDelegate() {
}

void ContextualSearchDelegate::StartSearchTermResolutionRequest(
    const std::string& selection,
    bool use_resolved_search_term,
    content::WebContents* web_contents,
    bool may_send_base_page_url) {
  GatherSurroundingTextWithCallback(
      selection, use_resolved_search_term, web_contents, may_send_base_page_url,
      base::Bind(&ContextualSearchDelegate::StartSearchTermRequestFromSelection,
                 AsWeakPtr()));
}

void ContextualSearchDelegate::GatherAndSaveSurroundingText(
    const std::string& selection,
    bool use_resolved_search_term,
    content::WebContents* web_contents,
    bool may_send_base_page_url) {
  GatherSurroundingTextWithCallback(
      selection, use_resolved_search_term, web_contents, may_send_base_page_url,
      base::Bind(&ContextualSearchDelegate::SaveSurroundingText, AsWeakPtr()));
  // TODO(donnd): clear the context here, since we're done with it (but risky).
}

void ContextualSearchDelegate::ContinueSearchTermResolutionRequest() {
  DCHECK(context_.get());
  if (!context_.get())
    return;
  GURL request_url(BuildRequestUrl(context_->selected_text));
  DCHECK(request_url.is_valid());

  // Reset will delete any previous fetcher, and we won't get any callback.
  search_term_fetcher_.reset(
      net::URLFetcher::Create(kContextualSearchURLFetcherID, request_url,
                              net::URLFetcher::GET, this).release());
  search_term_fetcher_->SetRequestContext(url_request_context_);

  // Add Chrome experiment state to the request headers.
  net::HttpRequestHeaders headers;
  // Note: It's fine to pass in |is_signed_in| false, which does not affect
  // transmission of experiment ids coming from the variations server.
  bool is_signed_in = false;
  variations::AppendVariationHeaders(
      search_term_fetcher_->GetOriginalURL(),
      false,  // Impossible to be incognito at this point.
      false, is_signed_in, &headers);
  search_term_fetcher_->SetExtraRequestHeaders(headers.ToString());

  SetDiscourseContextAndAddToHeader(*context_);

  search_term_fetcher_->Start();
}

void ContextualSearchDelegate::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(source == search_term_fetcher_.get());
  int response_code = source->GetResponseCode();

  std::unique_ptr<ResolvedSearchTerm> resolved_search_term(
      new ResolvedSearchTerm(response_code));
  if (source->GetStatus().is_success() && response_code == net::HTTP_OK) {
    std::string response;
    bool has_string_response = source->GetResponseAsString(&response);
    DCHECK(has_string_response);
    if (has_string_response) {
      resolved_search_term =
          GetResolvedSearchTermFromJson(response_code, response);
    }
  }
  search_term_callback_.Run(*resolved_search_term);

  // The ContextualSearchContext is consumed once the request has completed.
  context_.reset();
}

std::unique_ptr<ResolvedSearchTerm>
ContextualSearchDelegate::GetResolvedSearchTermFromJson(
    int response_code,
    const std::string& json_string) {
  std::string search_term;
  std::string display_text;
  std::string alternate_term;
  std::string mid;
  std::string prevent_preload;
  int mention_start = 0;
  int mention_end = 0;
  int start_adjust = 0;
  int end_adjust = 0;
  std::string context_language;
  std::string thumbnail_url = "";
  std::string caption = "";
  std::string quick_action_uri = "";
  QuickActionCategory quick_action_category = QUICK_ACTION_CATEGORY_NONE;

  DecodeSearchTermFromJsonResponse(
      json_string, &search_term, &display_text, &alternate_term, &mid,
      &prevent_preload, &mention_start, &mention_end, &context_language,
      &thumbnail_url, &caption, &quick_action_uri, &quick_action_category);
  if (mention_start != 0 || mention_end != 0) {
    // Sanity check that our selection is non-zero and it is less than
    // 100 characters as that would make contextual search bar hide.
    // We also check that there is at least one character overlap between
    // the new and old selection.
    if (mention_start >= mention_end ||
        (mention_end - mention_start) > kContextualSearchMaxSelection ||
        mention_end <= context_->start_offset ||
        mention_start >= context_->end_offset) {
      start_adjust = 0;
      end_adjust = 0;
    } else {
      start_adjust = mention_start - context_->start_offset;
      end_adjust = mention_end - context_->end_offset;
    }
  }
  bool is_invalid = response_code == net::URLFetcher::RESPONSE_CODE_INVALID;
  return std::unique_ptr<ResolvedSearchTerm>(new ResolvedSearchTerm(
      is_invalid, response_code, search_term, display_text, alternate_term, mid,
      prevent_preload == kDoPreventPreloadValue, start_adjust, end_adjust,
      context_language, thumbnail_url, caption, quick_action_uri,
      quick_action_category));
}

std::string ContextualSearchDelegate::BuildRequestUrl(std::string selection) {
  // TODO(donnd): Confirm this is the right way to handle TemplateURL fails.
  if (!template_url_service_ ||
      !template_url_service_->GetDefaultSearchProvider()) {
    return std::string();
  }

  std::string selected_text(net::EscapeQueryParamValue(selection, true));
  TemplateURL* template_url = template_url_service_->GetDefaultSearchProvider();

  TemplateURLRef::SearchTermsArgs search_terms_args =
      TemplateURLRef::SearchTermsArgs(base::string16());

  int contextual_cards_version = kContextualCardsNoIntegration;
  if (field_trial_->IsContextualCardsBarIntegrationEnabled())
    contextual_cards_version = kContextualCardsBarIntegration;
  if (base::FeatureList::IsEnabled(
          chrome::android::kContextualSearchSingleActions)) {
    contextual_cards_version = kContextualCardsSingleAction;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      kContextualCardsVersionOverride)){
    contextual_cards_version =
        std::stoi(base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kContextualCardsVersionOverride), nullptr);
  }

  TemplateURLRef::SearchTermsArgs::ContextualSearchParams params(
      kContextualSearchRequestVersion, selected_text, std::string(),
      contextual_cards_version);

  search_terms_args.contextual_search_params = params;

  std::string request(
      template_url->contextual_search_url_ref().ReplaceSearchTerms(
          search_terms_args,
          template_url_service_->search_terms_data(),
          NULL));

  // The switch/param should be the URL up to and including the endpoint.
  std::string replacement_url = field_trial_->GetResolverURLPrefix();

  // If a replacement URL was specified above, do the substitution.
  if (!replacement_url.empty()) {
    size_t pos = request.find(kContextualSearchServerEndpoint);
    if (pos != std::string::npos) {
      request.replace(0, pos + strlen(kContextualSearchServerEndpoint),
                      replacement_url);
    }
  }
  return request;
}

void ContextualSearchDelegate::GatherSurroundingTextWithCallback(
    const std::string& selection,
    bool use_resolved_search_term,
    content::WebContents* web_contents,
    bool may_send_base_page_url,
    HandleSurroundingsCallback callback) {
  // Immediately cancel any request that's in flight, since we're building a new
  // context (and the response disposes of any existing context).
  search_term_fetcher_.reset();
  BuildContext(selection, use_resolved_search_term, web_contents,
               may_send_base_page_url);
  DCHECK(web_contents);
  RenderFrameHost* focused_frame = web_contents->GetFocusedFrame();
  if (focused_frame) {
    focused_frame->RequestTextSurroundingSelection(
        callback, field_trial_->GetSurroundingSize());
  } else {
    callback.Run(base::string16(), 0, 0);
  }
}

void ContextualSearchDelegate::BuildContext(
    const std::string& selection,
    bool use_resolved_search_term,
    content::WebContents* web_contents,
    bool may_send_base_page_url) {
  // Decide if the URL should be sent with the context.
  GURL page_url(web_contents->GetURL());
  GURL url_to_send;
  if (may_send_base_page_url &&
      CanSendPageURL(page_url, ProfileManager::GetActiveUserProfile(),
                     template_url_service_)) {
    url_to_send = page_url;
  }
  std::string encoding(web_contents->GetEncoding());
  context_.reset(new ContextualSearchContext(
      selection, use_resolved_search_term, url_to_send, encoding));
}

void ContextualSearchDelegate::StartSearchTermRequestFromSelection(
    const base::string16& surrounding_text,
    int start_offset,
    int end_offset) {
  // TODO(donnd): figure out how to gather text surrounding the selection
  // for other purposes too: e.g. to determine if we should select the
  // word where the user tapped.
  if (context_.get()) {
    SaveSurroundingText(surrounding_text, start_offset, end_offset);
    SendSurroundingText(kSurroundingSizeForUI);
    ContinueSearchTermResolutionRequest();
  } else {
    DVLOG(1) << "ctxs: Null context, ignored!";
  }
}

void ContextualSearchDelegate::SaveSurroundingText(
    const base::string16& surrounding_text,
    int start_offset,
    int end_offset) {
  DCHECK(context_.get());
  // Sometimes the surroundings are 0, 0, '', so fall back on the selection.
  // See crbug.com/393100.
  if (start_offset == 0 && end_offset == 0 && surrounding_text.length() == 0) {
    context_->surrounding_text = base::UTF8ToUTF16(context_->selected_text);
    context_->start_offset = 0;
    context_->end_offset = context_->selected_text.length();
  } else {
    context_->surrounding_text = surrounding_text;
    context_->start_offset = start_offset;
    context_->end_offset = end_offset;
  }

  // Pin the start and end offsets to ensure they point within the string.
  int surrounding_length = context_->surrounding_text.length();
  context_->start_offset =
      std::min(surrounding_length, std::max(0, context_->start_offset));
  context_->end_offset =
      std::min(surrounding_length, std::max(0, context_->end_offset));

  // Call the Icing callback with a shortened copy of the surroundings.
  int icing_surrounding_size = field_trial_->GetIcingSurroundingSize();
  size_t selection_start = context_->start_offset;
  size_t selection_end = context_->end_offset;
  if (icing_surrounding_size >= 0 && selection_start < selection_end) {
    int icing_padding_each_side = icing_surrounding_size / 2;
    base::string16 icing_surrounding_text = SurroundingTextForIcing(
        context_->surrounding_text, icing_padding_each_side, &selection_start,
        &selection_end);
    if (selection_start < selection_end)
      icing_callback_.Run(context_->encoding, icing_surrounding_text,
                          selection_start, selection_end);
  }
}

void ContextualSearchDelegate::SendSurroundingText(int max_surrounding_chars) {
  const base::string16& surrounding = context_->surrounding_text;

  // Determine the text after the selection.
  int surrounding_length = surrounding.length();  // Cast to int.
  int num_after_characters = std::min(
      surrounding_length - context_->end_offset, max_surrounding_chars);
  base::string16 after_text = surrounding.substr(
      context_->end_offset, num_after_characters);

  base::TrimWhitespace(after_text, base::TRIM_ALL, &after_text);
  surrounding_callback_.Run(UTF16ToUTF8(after_text));
}

void ContextualSearchDelegate::SetDiscourseContextAndAddToHeader(
    const ContextualSearchContext& context) {
  search_term_fetcher_->AddExtraRequestHeader(GetDiscourseContext(context));
}

std::string ContextualSearchDelegate::GetDiscourseContext(
    const ContextualSearchContext& context) {
  discourse_context::ClientDiscourseContext proto;
  discourse_context::Display* display = proto.add_display();
  display->set_uri(context.page_url.spec());

  discourse_context::Media* media = display->mutable_media();
  media->set_mime_type(context.encoding);

  discourse_context::Selection* selection = display->mutable_selection();
  selection->set_content(UTF16ToUTF8(context.surrounding_text));
  selection->set_start(context.start_offset);
  selection->set_end(context.end_offset);
  selection->set_is_uri_encoded(false);

  std::string serialized;
  proto.SerializeToString(&serialized);

  std::string encoded_context;
  base::Base64Encode(serialized, &encoded_context);
  // The server memoizer expects a web-safe encoding.
  std::replace(encoded_context.begin(), encoded_context.end(), '+', '-');
  std::replace(encoded_context.begin(), encoded_context.end(), '/', '_');
  return kDiscourseContextHeaderPrefix + encoded_context;
}

bool ContextualSearchDelegate::CanSendPageURL(
    const GURL& current_page_url,
    Profile* profile,
    TemplateURLService* template_url_service) {
  // Check whether there is a Finch parameter preventing us from sending the
  // page URL.
  if (field_trial_->IsSendBasePageURLDisabled())
    return false;

  // Ensure that the default search provider is Google.
  TemplateURL* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  bool is_default_search_provider_google =
      default_search_provider &&
      default_search_provider->url_ref().HasGoogleBaseURLs(
          template_url_service->search_terms_data());
  if (!is_default_search_provider_google)
    return false;

  // Only allow HTTP URLs or HTTPS URLs.
  if (current_page_url.scheme() != url::kHttpScheme &&
      (current_page_url.scheme() != url::kHttpsScheme))
    return false;

  // Check that the user has sync enabled, is logged in, and syncs their Chrome
  // History.
  browser_sync::ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  syncer::SyncPrefs sync_prefs(profile->GetPrefs());
  if (service == NULL || !service->CanSyncStart() ||
      !sync_prefs.GetPreferredDataTypes(syncer::UserTypes())
           .Has(syncer::PROXY_TABS) ||
      !service->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES)) {
    return false;
  }

  return true;
}

// Gets the target language from the translate service using the user's profile.
std::string ContextualSearchDelegate::GetTargetLanguage() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  PrefService* pref_service = profile->GetPrefs();
  std::string result = TranslateService::GetTargetLanguage(pref_service);
  DCHECK(!result.empty());
  return result;
}

// Returns the accept languages preference string.
std::string ContextualSearchDelegate::GetAcceptLanguages() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  PrefService* pref_service = profile->GetPrefs();
  return pref_service->GetString(prefs::kAcceptLanguages);
}

// Decodes the given response from the search term resolution request and sets
// the value of the given parameters.
void ContextualSearchDelegate::DecodeSearchTermFromJsonResponse(
    const std::string& response,
    std::string* search_term,
    std::string* display_text,
    std::string* alternate_term,
    std::string* mid,
    std::string* prevent_preload,
    int* mention_start,
    int* mention_end,
    std::string* lang,
    std::string* thumbnail_url,
    std::string* caption,
    std::string* quick_action_uri,
    QuickActionCategory* quick_action_category) {
  bool contains_xssi_escape =
      base::StartsWith(response, kXssiEscape, base::CompareCase::SENSITIVE);
  const std::string& proper_json =
      contains_xssi_escape ? response.substr(sizeof(kXssiEscape) - 1)
                           : response;
  JSONStringValueDeserializer deserializer(proper_json);
  std::unique_ptr<base::Value> root =
      deserializer.Deserialize(nullptr, nullptr);
  const std::unique_ptr<base::DictionaryValue> dict =
      base::DictionaryValue::From(std::move(root));
  if (!dict)
    return;

  dict->GetString(kContextualSearchPreventPreload, prevent_preload);
  dict->GetString(kContextualSearchResponseSearchTermParam, search_term);
  dict->GetString(kContextualSearchResponseLanguageParam, lang);

  // For the display_text, if not present fall back to the "search_term".
  if (!dict->GetString(kContextualSearchResponseDisplayTextParam,
                       display_text)) {
    *display_text = *search_term;
  }
  dict->GetString(kContextualSearchResponseMidParam, mid);

  // Extract mentions for selection expansion.
  if (!field_trial_->IsDecodeMentionsDisabled()) {
    base::ListValue* mentions_list = nullptr;
    dict->GetList(kContextualSearchMentions, &mentions_list);
    if (mentions_list && mentions_list->GetSize() >= 2)
      ExtractMentionsStartEnd(*mentions_list, mention_start, mention_end);
  }

  // If either the selected text or the resolved term is not the search term,
  // use it as the alternate term.
  std::string selected_text;
  dict->GetString(kContextualSearchResponseSelectedTextParam, &selected_text);
  if (selected_text != *search_term) {
    *alternate_term = selected_text;
  } else {
    std::string resolved_term;
    dict->GetString(kContextualSearchResponseResolvedTermParam, &resolved_term);
    if (resolved_term != *search_term) {
      *alternate_term = resolved_term;
    }
  }

  if (field_trial_->IsContextualCardsBarIntegrationEnabled()) {
    // Contextual Cards V1 Integration.
    // Get the basic Bar data for Contextual Cards integration directly
    // from the root.
    dict->GetString(kContextualSearchCaption, caption);
    dict->GetString(kContextualSearchThumbnail, thumbnail_url);
  }

  if (base::FeatureList::IsEnabled(
          chrome::android::kContextualSearchSingleActions)) {
    // Contextual Cards V2 Integration.
    // Get the Single Action data.
    dict->GetString(kContextualSearchAction, quick_action_uri);
    std::string quick_action_category_string;
    dict->GetString(kContextualSearchCategory, &quick_action_category_string);
    if (!quick_action_category_string.empty()) {
      if (quick_action_category_string == kActionCategoryAddress) {
        *quick_action_category = QUICK_ACTION_CATEGORY_ADDRESS;
      } else if (quick_action_category_string == kActionCategoryEmail) {
        *quick_action_category = QUICK_ACTION_CATEGORY_EMAIL;
      } else if (quick_action_category_string == kActionCategoryEvent) {
        *quick_action_category = QUICK_ACTION_CATEGORY_EVENT;
      } else if (quick_action_category_string == kActionCategoryPhone) {
        *quick_action_category = QUICK_ACTION_CATEGORY_PHONE;
      }
    }
  }

  if (field_trial_->IsContextualCardsBarIntegrationEnabled() ||
      base::FeatureList::IsEnabled(
          chrome::android::kContextualSearchSingleActions)) {
    // Any Contextual Cards integration.
    // For testing purposes check if there was a diagnostic from Contextual
    // Cards and output that into the log.
    // TODO(donnd): remove after full Contextual Cards integration.
    std::string contextual_cards_diagnostic;
    dict->GetString("diagnostic", &contextual_cards_diagnostic);
    if (contextual_cards_diagnostic.empty()) {
      DVLOG(0) << "No diagnostic data in the response.";
    } else {
      DVLOG(0) << "The Contextual Cards backend response: ";
      DVLOG(0) << contextual_cards_diagnostic;
    }
  }
}

// Extract the Start/End of the mentions in the surrounding text
// for selection-expansion.
void ContextualSearchDelegate::ExtractMentionsStartEnd(
    const base::ListValue& mentions_list,
    int* startResult,
    int* endResult) {
  int int_value;
  if (mentions_list.GetInteger(0, &int_value))
    *startResult = std::max(0, int_value);
  if (mentions_list.GetInteger(1, &int_value))
    *endResult = std::max(0, int_value);
}

base::string16 ContextualSearchDelegate::SurroundingTextForIcing(
    const base::string16& surrounding_text,
    int padding_each_side,
    size_t* start,
    size_t* end) {
  base::string16 result_text = surrounding_text;
  size_t start_offset = *start;
  size_t end_offset = *end;
  size_t padding_each_side_pinned =
      padding_each_side >= 0 ? padding_each_side : 0;
  // Now trim the context so the portions before or after the selection
  // are within the given limit.
  if (start_offset > padding_each_side_pinned) {
    // Trim the start.
    int trim = start_offset - padding_each_side_pinned;
    result_text = result_text.substr(trim);
    start_offset -= trim;
    end_offset -= trim;
  }
  if (result_text.length() > end_offset + padding_each_side_pinned) {
    // Trim the end.
    result_text = result_text.substr(0, end_offset + padding_each_side_pinned);
  }
  *start = start_offset;
  *end = end_offset;
  return result_text;
}
