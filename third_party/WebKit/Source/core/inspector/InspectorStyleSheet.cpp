/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "core/inspector/InspectorStyleSheet.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptRegexp.h"
#include "core/CSSPropertyNames.h"
#include "core/css/CSSImportRule.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSMediaRule.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/CSSSupportsRule.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/parser/CSSParserObserver.h"
#include "core/dom/DOMNodeIds.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/StyleEngine.h"
#include "core/html/HTMLStyleElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorNetworkAgent.h"
#include "core/inspector/InspectorResourceContainer.h"
#include "core/svg/SVGStyleElement.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/TextPosition.h"
#include <algorithm>

using blink::protocol::Array;
using blink::RuleSourceDataList;
using blink::CSSRuleSourceData;
using blink::CSSStyleSheet;

namespace {

using namespace blink;

static CSSParserContext parserContextForDocument(Document* document) {
  return document ? CSSParserContext(*document, nullptr)
                  : strictCSSParserContext();
}

String findMagicComment(const String& content, const String& name) {
  DCHECK(name.find("=") == kNotFound);

  unsigned length = content.length();
  unsigned nameLength = name.length();
  const bool multiline = true;

  size_t pos = length;
  size_t equalSignPos = 0;
  size_t closingCommentPos = 0;
  while (true) {
    pos = content.reverseFind(name, pos);
    if (pos == kNotFound)
      return emptyString();

    // Check for a /\/[\/*][@#][ \t]/ regexp (length of 4) before found name.
    if (pos < 4)
      return emptyString();
    pos -= 4;
    if (content[pos] != '/')
      continue;
    if ((content[pos + 1] != '/' || multiline) &&
        (content[pos + 1] != '*' || !multiline))
      continue;
    if (content[pos + 2] != '#' && content[pos + 2] != '@')
      continue;
    if (content[pos + 3] != ' ' && content[pos + 3] != '\t')
      continue;
    equalSignPos = pos + 4 + nameLength;
    if (equalSignPos < length && content[equalSignPos] != '=')
      continue;
    if (multiline) {
      closingCommentPos = content.find("*/", equalSignPos + 1);
      if (closingCommentPos == kNotFound)
        return emptyString();
    }

    break;
  }

  DCHECK(equalSignPos);
  DCHECK(!multiline || closingCommentPos);
  size_t urlPos = equalSignPos + 1;
  String match = multiline
                     ? content.substring(urlPos, closingCommentPos - urlPos)
                     : content.substring(urlPos);

  size_t newLine = match.find("\n");
  if (newLine != kNotFound)
    match = match.substring(0, newLine);
  match = match.stripWhiteSpace();

  String disallowedChars("\"' \t");
  for (unsigned i = 0; i < match.length(); ++i) {
    if (disallowedChars.find(match[i]) != kNotFound)
      return emptyString();
  }

  return match;
}

void getClassNamesFromRule(CSSStyleRule* rule, HashSet<String>& uniqueNames) {
  const CSSSelectorList& selectorList = rule->styleRule()->selectorList();
  if (!selectorList.isValid())
    return;

  for (const CSSSelector* subSelector = selectorList.first(); subSelector;
       subSelector = CSSSelectorList::next(*subSelector)) {
    const CSSSelector* simpleSelector = subSelector;
    while (simpleSelector) {
      if (simpleSelector->match() == CSSSelector::Class)
        uniqueNames.add(simpleSelector->value());
      simpleSelector = simpleSelector->tagHistory();
    }
  }
}

class StyleSheetHandler final : public CSSParserObserver {
 public:
  StyleSheetHandler(const String& parsedText,
                    Document* document,
                    RuleSourceDataList* result)
      : m_parsedText(parsedText),
        m_document(document),
        m_result(result),
        m_currentRuleData(nullptr) {
    ASSERT(m_result);
  }

 private:
  void startRuleHeader(StyleRule::RuleType, unsigned) override;
  void endRuleHeader(unsigned) override;
  void observeSelector(unsigned startOffset, unsigned endOffset) override;
  void startRuleBody(unsigned) override;
  void endRuleBody(unsigned) override;
  void observeProperty(unsigned startOffset,
                       unsigned endOffset,
                       bool isImportant,
                       bool isParsed) override;
  void observeComment(unsigned startOffset, unsigned endOffset) override;

  void addNewRuleToSourceTree(PassRefPtr<CSSRuleSourceData>);
  PassRefPtr<CSSRuleSourceData> popRuleData();
  template <typename CharacterType>
  inline void setRuleHeaderEnd(const CharacterType*, unsigned);
  void fixUnparsedPropertyRanges(CSSRuleSourceData*);

  const String& m_parsedText;
  Member<Document> m_document;
  RuleSourceDataList* m_result;
  RuleSourceDataList m_currentRuleDataStack;
  CSSRuleSourceData* m_currentRuleData;
};

void StyleSheetHandler::startRuleHeader(StyleRule::RuleType type,
                                        unsigned offset) {
  // Pop off data for a previous invalid rule.
  if (m_currentRuleData)
    m_currentRuleDataStack.pop_back();

  RefPtr<CSSRuleSourceData> data = CSSRuleSourceData::create(type);
  data->ruleHeaderRange.start = offset;
  m_currentRuleData = data.get();
  m_currentRuleDataStack.push_back(data.release());
}

template <typename CharacterType>
inline void StyleSheetHandler::setRuleHeaderEnd(const CharacterType* dataStart,
                                                unsigned listEndOffset) {
  while (listEndOffset > 1) {
    if (isHTMLSpace<CharacterType>(*(dataStart + listEndOffset - 1)))
      --listEndOffset;
    else
      break;
  }

  m_currentRuleDataStack.back()->ruleHeaderRange.end = listEndOffset;
  if (!m_currentRuleDataStack.back()->selectorRanges.isEmpty())
    m_currentRuleDataStack.back()->selectorRanges.back().end = listEndOffset;
}

void StyleSheetHandler::endRuleHeader(unsigned offset) {
  ASSERT(!m_currentRuleDataStack.isEmpty());

  if (m_parsedText.is8Bit())
    setRuleHeaderEnd<LChar>(m_parsedText.characters8(), offset);
  else
    setRuleHeaderEnd<UChar>(m_parsedText.characters16(), offset);
}

void StyleSheetHandler::observeSelector(unsigned startOffset,
                                        unsigned endOffset) {
  ASSERT(m_currentRuleDataStack.size());
  m_currentRuleDataStack.back()->selectorRanges.push_back(
      SourceRange(startOffset, endOffset));
}

void StyleSheetHandler::startRuleBody(unsigned offset) {
  m_currentRuleData = nullptr;
  ASSERT(!m_currentRuleDataStack.isEmpty());
  if (m_parsedText[offset] == '{')
    ++offset;  // Skip the rule body opening brace.
  m_currentRuleDataStack.back()->ruleBodyRange.start = offset;
}

void StyleSheetHandler::endRuleBody(unsigned offset) {
  ASSERT(!m_currentRuleDataStack.isEmpty());
  m_currentRuleDataStack.back()->ruleBodyRange.end = offset;
  RefPtr<CSSRuleSourceData> rule = popRuleData();

  fixUnparsedPropertyRanges(rule.get());
  addNewRuleToSourceTree(rule.release());
}

void StyleSheetHandler::addNewRuleToSourceTree(
    PassRefPtr<CSSRuleSourceData> rule) {
  if (m_currentRuleDataStack.isEmpty())
    m_result->push_back(rule);
  else
    m_currentRuleDataStack.back()->childRules.push_back(rule);
}

PassRefPtr<CSSRuleSourceData> StyleSheetHandler::popRuleData() {
  ASSERT(!m_currentRuleDataStack.isEmpty());
  m_currentRuleData = nullptr;
  RefPtr<CSSRuleSourceData> data = m_currentRuleDataStack.back().get();
  m_currentRuleDataStack.pop_back();
  return data.release();
}

template <typename CharacterType>
static inline void fixUnparsedProperties(const CharacterType* characters,
                                         CSSRuleSourceData* ruleData) {
  Vector<CSSPropertySourceData>& propertyData =
      ruleData->styleSourceData->propertyData;
  unsigned size = propertyData.size();
  if (!size)
    return;

  CSSPropertySourceData* nextData = &(propertyData.at(0));
  for (unsigned i = 0; i < size; ++i) {
    CSSPropertySourceData* currentData = nextData;
    nextData = i < size - 1 ? &(propertyData.at(i + 1)) : nullptr;

    if (currentData->parsedOk)
      continue;
    if (currentData->range.end > 0 &&
        characters[currentData->range.end - 1] == ';')
      continue;

    unsigned propertyEnd;
    if (!nextData)
      propertyEnd = ruleData->ruleBodyRange.end - 1;
    else
      propertyEnd = nextData->range.start - 1;

    while (isHTMLSpace<CharacterType>(characters[propertyEnd]))
      --propertyEnd;

    // propertyEnd points at the last property text character.
    unsigned newPropertyEnd =
        propertyEnd + 1;  // Exclusive of the last property text character.
    if (currentData->range.end != newPropertyEnd) {
      currentData->range.end = newPropertyEnd;
      unsigned valueStart =
          currentData->range.start + currentData->name.length();
      while (valueStart < propertyEnd && characters[valueStart] != ':')
        ++valueStart;
      if (valueStart < propertyEnd)
        ++valueStart;  // Shift past the ':'.
      while (valueStart < propertyEnd &&
             isHTMLSpace<CharacterType>(characters[valueStart]))
        ++valueStart;
      // Need to exclude the trailing ';' from the property value.
      currentData->value = String(
          characters + valueStart,
          propertyEnd - valueStart + (characters[propertyEnd] == ';' ? 0 : 1));
    }
  }
}

void StyleSheetHandler::fixUnparsedPropertyRanges(CSSRuleSourceData* ruleData) {
  if (!ruleData->styleSourceData)
    return;

  if (m_parsedText.is8Bit()) {
    fixUnparsedProperties<LChar>(m_parsedText.characters8(), ruleData);
    return;
  }

  fixUnparsedProperties<UChar>(m_parsedText.characters16(), ruleData);
}

void StyleSheetHandler::observeProperty(unsigned startOffset,
                                        unsigned endOffset,
                                        bool isImportant,
                                        bool isParsed) {
  if (m_currentRuleDataStack.isEmpty() ||
      !m_currentRuleDataStack.back()->styleSourceData)
    return;

  ASSERT(endOffset <= m_parsedText.length());
  if (endOffset < m_parsedText.length() &&
      m_parsedText[endOffset] ==
          ';')  // Include semicolon into the property text.
    ++endOffset;

  ASSERT(startOffset < endOffset);
  String propertyString =
      m_parsedText.substring(startOffset, endOffset - startOffset)
          .stripWhiteSpace();
  if (propertyString.endsWith(';'))
    propertyString = propertyString.left(propertyString.length() - 1);
  size_t colonIndex = propertyString.find(':');
  ASSERT(colonIndex != kNotFound);

  String name = propertyString.left(colonIndex).stripWhiteSpace();
  String value =
      propertyString.substring(colonIndex + 1, propertyString.length())
          .stripWhiteSpace();
  m_currentRuleDataStack.back()->styleSourceData->propertyData.push_back(
      CSSPropertySourceData(name, value, isImportant, false, isParsed,
                            SourceRange(startOffset, endOffset)));
}

void StyleSheetHandler::observeComment(unsigned startOffset,
                                       unsigned endOffset) {
  ASSERT(endOffset <= m_parsedText.length());

  if (m_currentRuleDataStack.isEmpty() ||
      !m_currentRuleDataStack.back()->ruleHeaderRange.end ||
      !m_currentRuleDataStack.back()->styleSourceData)
    return;

  // The lexer is not inside a property AND it is scanning a declaration-aware
  // rule body.
  String commentText =
      m_parsedText.substring(startOffset, endOffset - startOffset);

  ASSERT(commentText.startsWith("/*"));
  commentText = commentText.substring(2);

  // Require well-formed comments.
  if (!commentText.endsWith("*/"))
    return;
  commentText =
      commentText.substring(0, commentText.length() - 2).stripWhiteSpace();
  if (commentText.isEmpty())
    return;

  // FIXME: Use the actual rule type rather than STYLE_RULE?
  RuleSourceDataList sourceData;

  StyleSheetHandler handler(commentText, m_document, &sourceData);
  CSSParser::parseDeclarationListForInspector(
      parserContextForDocument(m_document), commentText, handler);
  Vector<CSSPropertySourceData>& commentPropertyData =
      sourceData.front()->styleSourceData->propertyData;
  if (commentPropertyData.size() != 1)
    return;
  CSSPropertySourceData& propertyData = commentPropertyData.at(0);
  bool parsedOk = propertyData.parsedOk ||
                  propertyData.name.startsWith("-moz-") ||
                  propertyData.name.startsWith("-o-") ||
                  propertyData.name.startsWith("-webkit-") ||
                  propertyData.name.startsWith("-ms-");
  if (!parsedOk || propertyData.range.length() != commentText.length())
    return;

  m_currentRuleDataStack.back()->styleSourceData->propertyData.push_back(
      CSSPropertySourceData(propertyData.name, propertyData.value, false, true,
                            true, SourceRange(startOffset, endOffset)));
}

bool verifyRuleText(Document* document, const String& ruleText) {
  DEFINE_STATIC_LOCAL(String, bogusPropertyName, ("-webkit-boguz-propertee"));
  StyleSheetContents* styleSheet =
      StyleSheetContents::create(strictCSSParserContext());
  RuleSourceDataList sourceData;
  String text = ruleText + " div { " + bogusPropertyName + ": none; }";
  StyleSheetHandler handler(text, document, &sourceData);
  CSSParser::parseSheetForInspector(parserContextForDocument(document),
                                    styleSheet, text, handler);
  unsigned ruleCount = sourceData.size();

  // Exactly two rules should be parsed.
  if (ruleCount != 2)
    return false;

  // Added rule must be style rule.
  if (!sourceData.at(0)->styleSourceData)
    return false;

  Vector<CSSPropertySourceData>& propertyData =
      sourceData.at(1)->styleSourceData->propertyData;
  unsigned propertyCount = propertyData.size();

  // Exactly one property should be in rule.
  if (propertyCount != 1)
    return false;

  // Check for the property name.
  if (propertyData.at(0).name != bogusPropertyName)
    return false;

  return true;
}

bool verifyStyleText(Document* document, const String& text) {
  return verifyRuleText(document, "div {" + text + "}");
}

bool verifyKeyframeKeyText(Document* document, const String& keyText) {
  StyleSheetContents* styleSheet =
      StyleSheetContents::create(strictCSSParserContext());
  RuleSourceDataList sourceData;
  String text = "@keyframes boguzAnim { " + keyText +
                " { -webkit-boguz-propertee : none; } }";
  StyleSheetHandler handler(text, document, &sourceData);
  CSSParser::parseSheetForInspector(parserContextForDocument(document),
                                    styleSheet, text, handler);

  // Exactly two should be parsed.
  unsigned ruleCount = sourceData.size();
  if (ruleCount != 2 || sourceData.at(0)->type != StyleRule::Keyframes ||
      sourceData.at(1)->type != StyleRule::Keyframe)
    return false;

  // Exactly one property should be in keyframe rule.
  Vector<CSSPropertySourceData>& propertyData =
      sourceData.at(1)->styleSourceData->propertyData;
  unsigned propertyCount = propertyData.size();
  if (propertyCount != 1)
    return false;

  return true;
}

bool verifySelectorText(Document* document, const String& selectorText) {
  DEFINE_STATIC_LOCAL(String, bogusPropertyName, ("-webkit-boguz-propertee"));
  StyleSheetContents* styleSheet =
      StyleSheetContents::create(strictCSSParserContext());
  RuleSourceDataList sourceData;
  String text = selectorText + " { " + bogusPropertyName + ": none; }";
  StyleSheetHandler handler(text, document, &sourceData);
  CSSParser::parseSheetForInspector(parserContextForDocument(document),
                                    styleSheet, text, handler);

  // Exactly one rule should be parsed.
  unsigned ruleCount = sourceData.size();
  if (ruleCount != 1 || sourceData.at(0)->type != StyleRule::Style)
    return false;

  // Exactly one property should be in style rule.
  Vector<CSSPropertySourceData>& propertyData =
      sourceData.at(0)->styleSourceData->propertyData;
  unsigned propertyCount = propertyData.size();
  if (propertyCount != 1)
    return false;

  // Check for the property name.
  if (propertyData.at(0).name != bogusPropertyName)
    return false;

  return true;
}

bool verifyMediaText(Document* document, const String& mediaText) {
  DEFINE_STATIC_LOCAL(String, bogusPropertyName, ("-webkit-boguz-propertee"));
  StyleSheetContents* styleSheet =
      StyleSheetContents::create(strictCSSParserContext());
  RuleSourceDataList sourceData;
  String text =
      "@media " + mediaText + " { div { " + bogusPropertyName + ": none; } }";
  StyleSheetHandler handler(text, document, &sourceData);
  CSSParser::parseSheetForInspector(parserContextForDocument(document),
                                    styleSheet, text, handler);

  // Exactly one media rule should be parsed.
  unsigned ruleCount = sourceData.size();
  if (ruleCount != 1 || sourceData.at(0)->type != StyleRule::Media)
    return false;

  // Media rule should have exactly one style rule child.
  RuleSourceDataList& childSourceData = sourceData.at(0)->childRules;
  ruleCount = childSourceData.size();
  if (ruleCount != 1 || !childSourceData.at(0)->styleSourceData)
    return false;

  // Exactly one property should be in style rule.
  Vector<CSSPropertySourceData>& propertyData =
      childSourceData.at(0)->styleSourceData->propertyData;
  unsigned propertyCount = propertyData.size();
  if (propertyCount != 1)
    return false;

  // Check for the property name.
  if (propertyData.at(0).name != bogusPropertyName)
    return false;

  return true;
}

void flattenSourceData(const RuleSourceDataList& dataList,
                       RuleSourceDataList* result) {
  for (size_t i = 0; i < dataList.size(); ++i) {
    const RefPtr<CSSRuleSourceData>& data = dataList.at(i);

    // The result->append()'ed types should be exactly the same as in
    // collectFlatRules().
    switch (data->type) {
      case StyleRule::Style:
      case StyleRule::Import:
      case StyleRule::Page:
      case StyleRule::FontFace:
      case StyleRule::Viewport:
      case StyleRule::Keyframe:
        result->push_back(data.get());
        break;
      case StyleRule::Media:
      case StyleRule::Supports:
      case StyleRule::Keyframes:
        result->push_back(data.get());
        flattenSourceData(data->childRules, result);
        break;
      default:
        break;
    }
  }
}

CSSRuleList* asCSSRuleList(CSSRule* rule) {
  if (!rule)
    return nullptr;

  if (rule->type() == CSSRule::kMediaRule)
    return toCSSMediaRule(rule)->cssRules();

  if (rule->type() == CSSRule::kSupportsRule)
    return toCSSSupportsRule(rule)->cssRules();

  if (rule->type() == CSSRule::kKeyframesRule)
    return toCSSKeyframesRule(rule)->cssRules();

  return nullptr;
}

template <typename RuleList>
void collectFlatRules(RuleList ruleList, CSSRuleVector* result) {
  if (!ruleList)
    return;

  for (unsigned i = 0, size = ruleList->length(); i < size; ++i) {
    CSSRule* rule = ruleList->item(i);

    // The result->append()'ed types should be exactly the same as in
    // flattenSourceData().
    switch (rule->type()) {
      case CSSRule::kStyleRule:
      case CSSRule::kImportRule:
      case CSSRule::kCharsetRule:
      case CSSRule::kPageRule:
      case CSSRule::kFontFaceRule:
      case CSSRule::kViewportRule:
      case CSSRule::kKeyframeRule:
        result->push_back(rule);
        break;
      case CSSRule::kMediaRule:
      case CSSRule::kSupportsRule:
      case CSSRule::kKeyframesRule:
        result->push_back(rule);
        collectFlatRules(asCSSRuleList(rule), result);
        break;
      default:
        break;
    }
  }
}

typedef HashMap<unsigned,
                unsigned,
                WTF::IntHash<unsigned>,
                WTF::UnsignedWithZeroKeyHashTraits<unsigned>>
    IndexMap;

void diff(const Vector<String>& listA,
          const Vector<String>& listB,
          IndexMap* aToB,
          IndexMap* bToA) {
  // Cut of common prefix.
  size_t startOffset = 0;
  while (startOffset < listA.size() && startOffset < listB.size()) {
    if (listA.at(startOffset) != listB.at(startOffset))
      break;
    aToB->set(startOffset, startOffset);
    bToA->set(startOffset, startOffset);
    ++startOffset;
  }

  // Cut of common suffix.
  size_t endOffset = 0;
  while (endOffset < listA.size() - startOffset &&
         endOffset < listB.size() - startOffset) {
    size_t indexA = listA.size() - endOffset - 1;
    size_t indexB = listB.size() - endOffset - 1;
    if (listA.at(indexA) != listB.at(indexB))
      break;
    aToB->set(indexA, indexB);
    bToA->set(indexB, indexA);
    ++endOffset;
  }

  int n = listA.size() - startOffset - endOffset;
  int m = listB.size() - startOffset - endOffset;

  // If we mapped either of arrays, we have no more work to do.
  if (n == 0 || m == 0)
    return;

  int** diff = new int*[n];
  int** backtrack = new int*[n];
  for (int i = 0; i < n; ++i) {
    diff[i] = new int[m];
    backtrack[i] = new int[m];
  }

  // Compute longest common subsequence of two cssom models.
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < m; ++j) {
      int max = 0;
      int track = 0;

      if (i > 0 && diff[i - 1][j] > max) {
        max = diff[i - 1][j];
        track = 1;
      }

      if (j > 0 && diff[i][j - 1] > max) {
        max = diff[i][j - 1];
        track = 2;
      }

      if (listA.at(i + startOffset) == listB.at(j + startOffset)) {
        int value = i > 0 && j > 0 ? diff[i - 1][j - 1] + 1 : 1;
        if (value > max) {
          max = value;
          track = 3;
        }
      }

      diff[i][j] = max;
      backtrack[i][j] = track;
    }
  }

  // Backtrack and add missing mapping.
  int i = n - 1, j = m - 1;
  while (i >= 0 && j >= 0 && backtrack[i][j]) {
    switch (backtrack[i][j]) {
      case 1:
        i -= 1;
        break;
      case 2:
        j -= 1;
        break;
      case 3:
        aToB->set(i + startOffset, j + startOffset);
        bToA->set(j + startOffset, i + startOffset);
        i -= 1;
        j -= 1;
        break;
      default:
        ASSERT_NOT_REACHED();
    }
  }

  for (int i = 0; i < n; ++i) {
    delete[] diff[i];
    delete[] backtrack[i];
  }
  delete[] diff;
  delete[] backtrack;
}

String canonicalCSSText(CSSRule* rule) {
  if (rule->type() != CSSRule::kStyleRule)
    return rule->cssText();
  CSSStyleRule* styleRule = toCSSStyleRule(rule);

  Vector<String> propertyNames;
  CSSStyleDeclaration* style = styleRule->style();
  for (unsigned i = 0; i < style->length(); ++i)
    propertyNames.push_back(style->item(i));

  std::sort(propertyNames.begin(), propertyNames.end(),
            WTF::codePointCompareLessThan);

  StringBuilder builder;
  builder.append(styleRule->selectorText());
  builder.append('{');
  for (unsigned i = 0; i < propertyNames.size(); ++i) {
    String name = propertyNames.at(i);
    builder.append(' ');
    builder.append(name);
    builder.append(':');
    builder.append(style->getPropertyValue(name));
    if (!style->getPropertyPriority(name).isEmpty()) {
      builder.append(' ');
      builder.append(style->getPropertyPriority(name));
    }
    builder.append(';');
  }
  builder.append('}');

  return builder.toString();
}

}  // namespace

namespace blink {

enum MediaListSource {
  MediaListSourceLinkedSheet,
  MediaListSourceInlineSheet,
  MediaListSourceMediaRule,
  MediaListSourceImportRule
};

std::unique_ptr<protocol::CSS::SourceRange>
InspectorStyleSheetBase::buildSourceRangeObject(const SourceRange& range) {
  const LineEndings* lineEndings = this->lineEndings();
  if (!lineEndings)
    return nullptr;
  TextPosition start =
      TextPosition::fromOffsetAndLineEndings(range.start, *lineEndings);
  TextPosition end =
      TextPosition::fromOffsetAndLineEndings(range.end, *lineEndings);

  std::unique_ptr<protocol::CSS::SourceRange> result =
      protocol::CSS::SourceRange::create()
          .setStartLine(start.m_line.zeroBasedInt())
          .setStartColumn(start.m_column.zeroBasedInt())
          .setEndLine(end.m_line.zeroBasedInt())
          .setEndColumn(end.m_column.zeroBasedInt())
          .build();
  return result;
}

InspectorStyle* InspectorStyle::create(
    CSSStyleDeclaration* style,
    PassRefPtr<CSSRuleSourceData> sourceData,
    InspectorStyleSheetBase* parentStyleSheet) {
  return new InspectorStyle(style, std::move(sourceData), parentStyleSheet);
}

InspectorStyle::InspectorStyle(CSSStyleDeclaration* style,
                               PassRefPtr<CSSRuleSourceData> sourceData,
                               InspectorStyleSheetBase* parentStyleSheet)
    : m_style(style),
      m_sourceData(std::move(sourceData)),
      m_parentStyleSheet(parentStyleSheet) {
  ASSERT(m_style);
}

InspectorStyle::~InspectorStyle() {}

std::unique_ptr<protocol::CSS::CSSStyle> InspectorStyle::buildObjectForStyle() {
  std::unique_ptr<protocol::CSS::CSSStyle> result = styleWithProperties();
  if (m_sourceData) {
    if (m_parentStyleSheet && !m_parentStyleSheet->id().isEmpty())
      result->setStyleSheetId(m_parentStyleSheet->id());
    result->setRange(m_parentStyleSheet->buildSourceRangeObject(
        m_sourceData->ruleBodyRange));
    String sheetText;
    bool success = m_parentStyleSheet->getText(&sheetText);
    if (success) {
      const SourceRange& bodyRange = m_sourceData->ruleBodyRange;
      result->setCssText(sheetText.substring(bodyRange.start,
                                             bodyRange.end - bodyRange.start));
    }
  }

  return result;
}

bool InspectorStyle::styleText(String* result) {
  if (!m_sourceData)
    return false;

  return textForRange(m_sourceData->ruleBodyRange, result);
}

bool InspectorStyle::textForRange(const SourceRange& range, String* result) {
  String styleSheetText;
  bool success = m_parentStyleSheet->getText(&styleSheetText);
  if (!success)
    return false;

  ASSERT(0 <= range.start);
  ASSERT(range.start <= range.end);
  ASSERT(range.end <= styleSheetText.length());
  *result = styleSheetText.substring(range.start, range.end - range.start);
  return true;
}

void InspectorStyle::populateAllProperties(
    Vector<CSSPropertySourceData>& result) {
  HashSet<String> sourcePropertyNames;

  if (m_sourceData && m_sourceData->styleSourceData) {
    Vector<CSSPropertySourceData>& sourcePropertyData =
        m_sourceData->styleSourceData->propertyData;
    for (const auto& data : sourcePropertyData) {
      result.push_back(data);
      sourcePropertyNames.add(data.name.lower());
    }
  }

  for (int i = 0, size = m_style->length(); i < size; ++i) {
    String name = m_style->item(i);
    if (!sourcePropertyNames.add(name.lower()).isNewEntry)
      continue;

    String value = m_style->getPropertyValue(name);
    if (value.isEmpty())
      continue;
    result.push_back(CSSPropertySourceData(
        name, value, !m_style->getPropertyPriority(name).isEmpty(), false, true,
        SourceRange()));
  }
}

std::unique_ptr<protocol::CSS::CSSStyle> InspectorStyle::styleWithProperties() {
  std::unique_ptr<Array<protocol::CSS::CSSProperty>> propertiesObject =
      Array<protocol::CSS::CSSProperty>::create();
  std::unique_ptr<Array<protocol::CSS::ShorthandEntry>> shorthandEntries =
      Array<protocol::CSS::ShorthandEntry>::create();
  HashSet<String> foundShorthands;

  Vector<CSSPropertySourceData> properties;
  populateAllProperties(properties);

  for (auto& styleProperty : properties) {
    const CSSPropertySourceData& propertyEntry = styleProperty;
    const String& name = propertyEntry.name;

    std::unique_ptr<protocol::CSS::CSSProperty> property =
        protocol::CSS::CSSProperty::create()
            .setName(name)
            .setValue(propertyEntry.value)
            .build();

    // Default "parsedOk" == true.
    if (!propertyEntry.parsedOk)
      property->setParsedOk(false);
    String text;
    if (styleProperty.range.length() &&
        textForRange(styleProperty.range, &text))
      property->setText(text);
    if (propertyEntry.important)
      property->setImportant(true);
    if (styleProperty.range.length()) {
      property->setRange(
          m_parentStyleSheet
              ? m_parentStyleSheet->buildSourceRangeObject(propertyEntry.range)
              : nullptr);
      if (!propertyEntry.disabled) {
        property->setImplicit(false);
      }
      property->setDisabled(propertyEntry.disabled);
    } else if (!propertyEntry.disabled) {
      bool implicit = m_style->isPropertyImplicit(name);
      // Default "implicit" == false.
      if (implicit)
        property->setImplicit(true);

      String shorthand = m_style->getPropertyShorthand(name);
      if (!shorthand.isEmpty()) {
        if (foundShorthands.add(shorthand).isNewEntry) {
          std::unique_ptr<protocol::CSS::ShorthandEntry> entry =
              protocol::CSS::ShorthandEntry::create()
                  .setName(shorthand)
                  .setValue(shorthandValue(shorthand))
                  .build();
          if (!m_style->getPropertyPriority(name).isEmpty())
            entry->setImportant(true);
          shorthandEntries->addItem(std::move(entry));
        }
      }
    }
    propertiesObject->addItem(std::move(property));
  }

  std::unique_ptr<protocol::CSS::CSSStyle> result =
      protocol::CSS::CSSStyle::create()
          .setCssProperties(std::move(propertiesObject))
          .setShorthandEntries(std::move(shorthandEntries))
          .build();
  return result;
}

String InspectorStyle::shorthandValue(const String& shorthandProperty) {
  StringBuilder builder;
  String value = m_style->getPropertyValue(shorthandProperty);
  if (value.isEmpty()) {
    for (unsigned i = 0; i < m_style->length(); ++i) {
      String individualProperty = m_style->item(i);
      if (m_style->getPropertyShorthand(individualProperty) !=
          shorthandProperty)
        continue;
      if (m_style->isPropertyImplicit(individualProperty))
        continue;
      String individualValue = m_style->getPropertyValue(individualProperty);
      if (individualValue == "initial")
        continue;
      if (!builder.isEmpty())
        builder.append(' ');
      builder.append(individualValue);
    }
  } else {
    builder.append(value);
  }

  if (!m_style->getPropertyPriority(shorthandProperty).isEmpty())
    builder.append(" !important");

  return builder.toString();
}

DEFINE_TRACE(InspectorStyle) {
  visitor->trace(m_style);
  visitor->trace(m_parentStyleSheet);
}

InspectorStyleSheetBase::InspectorStyleSheetBase(Listener* listener)
    : m_id(IdentifiersFactory::createIdentifier()),
      m_listener(listener),
      m_lineEndings(WTF::makeUnique<LineEndings>()) {}

void InspectorStyleSheetBase::onStyleSheetTextChanged() {
  m_lineEndings = WTF::makeUnique<LineEndings>();
  if (listener())
    listener()->styleSheetChanged(this);
}

std::unique_ptr<protocol::CSS::CSSStyle>
InspectorStyleSheetBase::buildObjectForStyle(CSSStyleDeclaration* style) {
  return inspectorStyle(style)->buildObjectForStyle();
}

const LineEndings* InspectorStyleSheetBase::lineEndings() {
  if (m_lineEndings->size() > 0)
    return m_lineEndings.get();
  String text;
  if (getText(&text))
    m_lineEndings = WTF::lineEndings(text);
  return m_lineEndings.get();
}

bool InspectorStyleSheetBase::lineNumberAndColumnToOffset(unsigned lineNumber,
                                                          unsigned columnNumber,
                                                          unsigned* offset) {
  const LineEndings* endings = lineEndings();
  if (lineNumber >= endings->size())
    return false;
  unsigned charactersInLine =
      lineNumber > 0 ? endings->at(lineNumber) - endings->at(lineNumber - 1) - 1
                     : endings->at(0);
  if (columnNumber > charactersInLine)
    return false;
  TextPosition position(OrdinalNumber::fromZeroBasedInt(lineNumber),
                        OrdinalNumber::fromZeroBasedInt(columnNumber));
  *offset = position.toOffset(*endings).zeroBasedInt();
  return true;
}

InspectorStyleSheet* InspectorStyleSheet::create(
    InspectorNetworkAgent* networkAgent,
    CSSStyleSheet* pageStyleSheet,
    const String& origin,
    const String& documentURL,
    InspectorStyleSheetBase::Listener* listener,
    InspectorResourceContainer* resourceContainer) {
  return new InspectorStyleSheet(networkAgent, pageStyleSheet, origin,
                                 documentURL, listener, resourceContainer);
}

InspectorStyleSheet::InspectorStyleSheet(
    InspectorNetworkAgent* networkAgent,
    CSSStyleSheet* pageStyleSheet,
    const String& origin,
    const String& documentURL,
    InspectorStyleSheetBase::Listener* listener,
    InspectorResourceContainer* resourceContainer)
    : InspectorStyleSheetBase(listener),
      m_resourceContainer(resourceContainer),
      m_networkAgent(networkAgent),
      m_pageStyleSheet(pageStyleSheet),
      m_origin(origin),
      m_documentURL(documentURL) {
  String text;
  bool success = inspectorStyleSheetText(&text);
  if (!success)
    success = inlineStyleSheetText(&text);
  if (!success)
    success = resourceStyleSheetText(&text);
  if (success)
    innerSetText(text, false);
}

InspectorStyleSheet::~InspectorStyleSheet() {}

DEFINE_TRACE(InspectorStyleSheet) {
  visitor->trace(m_resourceContainer);
  visitor->trace(m_networkAgent);
  visitor->trace(m_pageStyleSheet);
  visitor->trace(m_cssomFlatRules);
  visitor->trace(m_parsedFlatRules);
  InspectorStyleSheetBase::trace(visitor);
}

static String styleSheetURL(CSSStyleSheet* pageStyleSheet) {
  if (pageStyleSheet && !pageStyleSheet->contents()->baseURL().isEmpty())
    return pageStyleSheet->contents()->baseURL().getString();
  return emptyString();
}

String InspectorStyleSheet::finalURL() {
  String url = styleSheetURL(m_pageStyleSheet.get());
  return url.isEmpty() ? m_documentURL : url;
}

bool InspectorStyleSheet::setText(const String& text, ExceptionState&) {
  innerSetText(text, true);
  m_pageStyleSheet->setText(text);
  onStyleSheetTextChanged();
  return true;
}

CSSStyleRule* InspectorStyleSheet::setRuleSelector(
    const SourceRange& range,
    const String& text,
    SourceRange* newRange,
    String* oldText,
    ExceptionState& exceptionState) {
  if (!verifySelectorText(m_pageStyleSheet->ownerDocument(), text)) {
    exceptionState.throwDOMException(SyntaxError,
                                     "Selector or media text is not valid.");
    return nullptr;
  }

  CSSRuleSourceData* sourceData = findRuleByHeaderRange(range);
  if (!sourceData || !sourceData->styleSourceData) {
    exceptionState.throwDOMException(
        NotFoundError, "Source range didn't match existing source range");
    return nullptr;
  }

  CSSRule* rule = ruleForSourceData(sourceData);
  if (!rule || !rule->parentStyleSheet() ||
      rule->type() != CSSRule::kStyleRule) {
    exceptionState.throwDOMException(
        NotFoundError, "Source range didn't match existing style source range");
    return nullptr;
  }

  CSSStyleRule* styleRule = InspectorCSSAgent::asCSSStyleRule(rule);
  styleRule->setSelectorText(text);

  replaceText(sourceData->ruleHeaderRange, text, newRange, oldText);
  onStyleSheetTextChanged();

  return styleRule;
}

CSSKeyframeRule* InspectorStyleSheet::setKeyframeKey(
    const SourceRange& range,
    const String& text,
    SourceRange* newRange,
    String* oldText,
    ExceptionState& exceptionState) {
  if (!verifyKeyframeKeyText(m_pageStyleSheet->ownerDocument(), text)) {
    exceptionState.throwDOMException(SyntaxError,
                                     "Keyframe key text is not valid.");
    return nullptr;
  }

  CSSRuleSourceData* sourceData = findRuleByHeaderRange(range);
  if (!sourceData || !sourceData->styleSourceData) {
    exceptionState.throwDOMException(
        NotFoundError, "Source range didn't match existing source range");
    return nullptr;
  }

  CSSRule* rule = ruleForSourceData(sourceData);
  if (!rule || !rule->parentStyleSheet() ||
      rule->type() != CSSRule::kKeyframeRule) {
    exceptionState.throwDOMException(
        NotFoundError, "Source range didn't match existing style source range");
    return nullptr;
  }

  CSSKeyframeRule* keyframeRule = toCSSKeyframeRule(rule);
  keyframeRule->setKeyText(text, exceptionState);

  replaceText(sourceData->ruleHeaderRange, text, newRange, oldText);
  onStyleSheetTextChanged();

  return keyframeRule;
}

CSSRule* InspectorStyleSheet::setStyleText(const SourceRange& range,
                                           const String& text,
                                           SourceRange* newRange,
                                           String* oldText,
                                           ExceptionState& exceptionState) {
  if (!verifyStyleText(m_pageStyleSheet->ownerDocument(), text)) {
    exceptionState.throwDOMException(SyntaxError, "Style text is not valid.");
    return nullptr;
  }

  CSSRuleSourceData* sourceData = findRuleByBodyRange(range);
  if (!sourceData || !sourceData->styleSourceData) {
    exceptionState.throwDOMException(
        NotFoundError, "Source range didn't match existing style source range");
    return nullptr;
  }

  CSSRule* rule = ruleForSourceData(sourceData);
  if (!rule || !rule->parentStyleSheet() ||
      (rule->type() != CSSRule::kStyleRule &&
       rule->type() != CSSRule::kKeyframeRule)) {
    exceptionState.throwDOMException(
        NotFoundError, "Source range didn't match existing style source range");
    return nullptr;
  }

  CSSStyleDeclaration* style = nullptr;
  if (rule->type() == CSSRule::kStyleRule)
    style = toCSSStyleRule(rule)->style();
  else if (rule->type() == CSSRule::kKeyframeRule)
    style = toCSSKeyframeRule(rule)->style();
  style->setCSSText(text, exceptionState);

  replaceText(sourceData->ruleBodyRange, text, newRange, oldText);
  onStyleSheetTextChanged();

  return rule;
}

CSSMediaRule* InspectorStyleSheet::setMediaRuleText(
    const SourceRange& range,
    const String& text,
    SourceRange* newRange,
    String* oldText,
    ExceptionState& exceptionState) {
  if (!verifyMediaText(m_pageStyleSheet->ownerDocument(), text)) {
    exceptionState.throwDOMException(SyntaxError,
                                     "Selector or media text is not valid.");
    return nullptr;
  }

  CSSRuleSourceData* sourceData = findRuleByHeaderRange(range);
  if (!sourceData || !sourceData->mediaSourceData) {
    exceptionState.throwDOMException(
        NotFoundError, "Source range didn't match existing source range");
    return nullptr;
  }

  CSSRule* rule = ruleForSourceData(sourceData);
  if (!rule || !rule->parentStyleSheet() ||
      rule->type() != CSSRule::kMediaRule) {
    exceptionState.throwDOMException(
        NotFoundError, "Source range didn't match existing style source range");
    return nullptr;
  }

  CSSMediaRule* mediaRule = InspectorCSSAgent::asCSSMediaRule(rule);
  mediaRule->media()->setMediaText(text);

  replaceText(sourceData->ruleHeaderRange, text, newRange, oldText);
  onStyleSheetTextChanged();

  return mediaRule;
}

CSSRuleSourceData* InspectorStyleSheet::ruleSourceDataAfterSourceRange(
    const SourceRange& sourceRange) {
  ASSERT(m_sourceData);
  unsigned index = 0;
  for (; index < m_sourceData->size(); ++index) {
    CSSRuleSourceData* sd = m_sourceData->at(index).get();
    if (sd->ruleHeaderRange.start >= sourceRange.end)
      break;
  }
  return index < m_sourceData->size() ? m_sourceData->at(index).get() : nullptr;
}

CSSStyleRule* InspectorStyleSheet::insertCSSOMRuleInStyleSheet(
    CSSRule* insertBefore,
    const String& ruleText,
    ExceptionState& exceptionState) {
  unsigned index = 0;
  for (; index < m_pageStyleSheet->length(); ++index) {
    CSSRule* rule = m_pageStyleSheet->item(index);
    if (rule == insertBefore)
      break;
  }

  m_pageStyleSheet->insertRule(ruleText, index, exceptionState);
  CSSRule* rule = m_pageStyleSheet->item(index);
  CSSStyleRule* styleRule = InspectorCSSAgent::asCSSStyleRule(rule);
  if (!styleRule) {
    m_pageStyleSheet->deleteRule(index, ASSERT_NO_EXCEPTION);
    exceptionState.throwDOMException(
        SyntaxError,
        "The rule '" + ruleText + "' could not be added in style sheet.");
    return nullptr;
  }
  return styleRule;
}

CSSStyleRule* InspectorStyleSheet::insertCSSOMRuleInMediaRule(
    CSSMediaRule* mediaRule,
    CSSRule* insertBefore,
    const String& ruleText,
    ExceptionState& exceptionState) {
  unsigned index = 0;
  for (; index < mediaRule->length(); ++index) {
    CSSRule* rule = mediaRule->item(index);
    if (rule == insertBefore)
      break;
  }

  mediaRule->insertRule(ruleText, index, exceptionState);
  CSSRule* rule = mediaRule->item(index);
  CSSStyleRule* styleRule = InspectorCSSAgent::asCSSStyleRule(rule);
  if (!styleRule) {
    mediaRule->deleteRule(index, ASSERT_NO_EXCEPTION);
    exceptionState.throwDOMException(
        SyntaxError,
        "The rule '" + ruleText + "' could not be added in media rule.");
    return nullptr;
  }
  return styleRule;
}

CSSStyleRule* InspectorStyleSheet::insertCSSOMRuleBySourceRange(
    const SourceRange& sourceRange,
    const String& ruleText,
    ExceptionState& exceptionState) {
  ASSERT(m_sourceData);

  CSSRuleSourceData* containingRuleSourceData = nullptr;
  for (size_t i = 0; i < m_sourceData->size(); ++i) {
    CSSRuleSourceData* ruleSourceData = m_sourceData->at(i).get();
    if (ruleSourceData->ruleHeaderRange.start < sourceRange.start &&
        sourceRange.start < ruleSourceData->ruleBodyRange.start) {
      exceptionState.throwDOMException(
          NotFoundError, "Cannot insert rule inside rule selector.");
      return nullptr;
    }
    if (sourceRange.start < ruleSourceData->ruleBodyRange.start ||
        ruleSourceData->ruleBodyRange.end < sourceRange.start)
      continue;
    if (!containingRuleSourceData ||
        containingRuleSourceData->ruleBodyRange.length() >
            ruleSourceData->ruleBodyRange.length())
      containingRuleSourceData = ruleSourceData;
  }

  CSSRuleSourceData* insertBefore = ruleSourceDataAfterSourceRange(sourceRange);
  CSSRule* insertBeforeRule = ruleForSourceData(insertBefore);

  if (!containingRuleSourceData)
    return insertCSSOMRuleInStyleSheet(insertBeforeRule, ruleText,
                                       exceptionState);

  CSSRule* rule = ruleForSourceData(containingRuleSourceData);
  if (!rule || rule->type() != CSSRule::kMediaRule) {
    exceptionState.throwDOMException(NotFoundError,
                                     "Cannot insert rule in non-media rule.");
    return nullptr;
  }

  return insertCSSOMRuleInMediaRule(toCSSMediaRule(rule), insertBeforeRule,
                                    ruleText, exceptionState);
}

CSSStyleRule* InspectorStyleSheet::addRule(const String& ruleText,
                                           const SourceRange& location,
                                           SourceRange* addedRange,
                                           ExceptionState& exceptionState) {
  if (location.start != location.end) {
    exceptionState.throwDOMException(NotFoundError,
                                     "Source range must be collapsed.");
    return nullptr;
  }

  if (!verifyRuleText(m_pageStyleSheet->ownerDocument(), ruleText)) {
    exceptionState.throwDOMException(SyntaxError, "Rule text is not valid.");
    return nullptr;
  }

  if (!m_sourceData) {
    exceptionState.throwDOMException(NotFoundError, "Style is read-only.");
    return nullptr;
  }

  CSSStyleRule* styleRule =
      insertCSSOMRuleBySourceRange(location, ruleText, exceptionState);
  if (exceptionState.hadException())
    return nullptr;

  replaceText(location, ruleText, addedRange, nullptr);
  onStyleSheetTextChanged();
  return styleRule;
}

bool InspectorStyleSheet::deleteRule(const SourceRange& range,
                                     ExceptionState& exceptionState) {
  if (!m_sourceData) {
    exceptionState.throwDOMException(NotFoundError, "Style is read-only.");
    return false;
  }

  // Find index of CSSRule that entirely belongs to the range.
  CSSRuleSourceData* foundData = nullptr;

  for (size_t i = 0; i < m_sourceData->size(); ++i) {
    CSSRuleSourceData* ruleSourceData = m_sourceData->at(i).get();
    unsigned ruleStart = ruleSourceData->ruleHeaderRange.start;
    unsigned ruleEnd = ruleSourceData->ruleBodyRange.end + 1;
    bool startBelongs = ruleStart >= range.start && ruleStart < range.end;
    bool endBelongs = ruleEnd > range.start && ruleEnd <= range.end;

    if (startBelongs != endBelongs)
      break;
    if (!startBelongs)
      continue;
    if (!foundData ||
        foundData->ruleBodyRange.length() >
            ruleSourceData->ruleBodyRange.length())
      foundData = ruleSourceData;
  }
  CSSRule* rule = ruleForSourceData(foundData);
  if (!rule) {
    exceptionState.throwDOMException(
        NotFoundError, "No style rule could be found in given range.");
    return false;
  }
  CSSStyleSheet* styleSheet = rule->parentStyleSheet();
  if (!styleSheet) {
    exceptionState.throwDOMException(NotFoundError,
                                     "No parent stylesheet could be found.");
    return false;
  }
  CSSRule* parentRule = rule->parentRule();
  if (parentRule) {
    if (parentRule->type() != CSSRule::kMediaRule) {
      exceptionState.throwDOMException(
          NotFoundError, "Cannot remove rule from non-media rule.");
      return false;
    }
    CSSMediaRule* parentMediaRule = toCSSMediaRule(parentRule);
    size_t index = 0;
    while (index < parentMediaRule->length() &&
           parentMediaRule->item(index) != rule)
      ++index;
    ASSERT(index < parentMediaRule->length());
    parentMediaRule->deleteRule(index, exceptionState);
  } else {
    size_t index = 0;
    while (index < styleSheet->length() && styleSheet->item(index) != rule)
      ++index;
    ASSERT(index < styleSheet->length());
    styleSheet->deleteRule(index, exceptionState);
  }
  // |rule| MAY NOT be addressed after this line!

  if (exceptionState.hadException())
    return false;

  replaceText(range, "", nullptr, nullptr);
  onStyleSheetTextChanged();
  return true;
}

std::unique_ptr<protocol::Array<String>>
InspectorStyleSheet::collectClassNames() {
  HashSet<String> uniqueNames;
  std::unique_ptr<protocol::Array<String>> result =
      protocol::Array<String>::create();

  for (size_t i = 0; i < m_parsedFlatRules.size(); ++i) {
    if (m_parsedFlatRules.at(i)->type() == CSSRule::kStyleRule)
      getClassNamesFromRule(toCSSStyleRule(m_parsedFlatRules.at(i)),
                            uniqueNames);
  }
  for (const String& className : uniqueNames)
    result->addItem(className);
  return result;
}

void InspectorStyleSheet::replaceText(const SourceRange& range,
                                      const String& text,
                                      SourceRange* newRange,
                                      String* oldText) {
  String sheetText = m_text;
  if (oldText)
    *oldText = sheetText.substring(range.start, range.length());
  sheetText.replace(range.start, range.length(), text);
  if (newRange)
    *newRange = SourceRange(range.start, range.start + text.length());
  innerSetText(sheetText, true);
}

void InspectorStyleSheet::innerSetText(const String& text,
                                       bool markAsLocallyModified) {
  RuleSourceDataList ruleTree;
  StyleSheetContents* styleSheet =
      StyleSheetContents::create(m_pageStyleSheet->contents()->parserContext());
  StyleSheetHandler handler(text, m_pageStyleSheet->ownerDocument(), &ruleTree);
  CSSParser::parseSheetForInspector(
      m_pageStyleSheet->contents()->parserContext(), styleSheet, text, handler);
  CSSStyleSheet* sourceDataSheet = nullptr;
  if (toCSSImportRule(m_pageStyleSheet->ownerRule()))
    sourceDataSheet = CSSStyleSheet::create(
        styleSheet, toCSSImportRule(m_pageStyleSheet->ownerRule()));
  else
    sourceDataSheet =
        CSSStyleSheet::create(styleSheet, *m_pageStyleSheet->ownerNode());

  m_parsedFlatRules.clear();
  collectFlatRules(sourceDataSheet, &m_parsedFlatRules);

  m_sourceData = WTF::makeUnique<RuleSourceDataList>();
  flattenSourceData(ruleTree, m_sourceData.get());
  m_text = text;

  if (markAsLocallyModified) {
    Element* element = ownerStyleElement();
    if (element)
      m_resourceContainer->storeStyleElementContent(
          DOMNodeIds::idForNode(element), text);
    else if (m_origin == protocol::CSS::StyleSheetOriginEnum::Inspector)
      m_resourceContainer->storeStyleElementContent(
          DOMNodeIds::idForNode(m_pageStyleSheet->ownerDocument()), text);
    else
      m_resourceContainer->storeStyleSheetContent(finalURL(), text);
  }
}

std::unique_ptr<protocol::CSS::CSSStyleSheetHeader>
InspectorStyleSheet::buildObjectForStyleSheetInfo() {
  CSSStyleSheet* styleSheet = pageStyleSheet();
  if (!styleSheet)
    return nullptr;

  Document* document = styleSheet->ownerDocument();
  LocalFrame* frame = document ? document->frame() : nullptr;

  std::unique_ptr<protocol::CSS::CSSStyleSheetHeader> result =
      protocol::CSS::CSSStyleSheetHeader::create()
          .setStyleSheetId(id())
          .setOrigin(m_origin)
          .setDisabled(styleSheet->disabled())
          .setSourceURL(url())
          .setTitle(styleSheet->title())
          .setFrameId(frame ? IdentifiersFactory::frameId(frame) : "")
          .setIsInline(styleSheet->isInline() && !startsAtZero())
          .setStartLine(
              styleSheet->startPositionInSource().m_line.zeroBasedInt())
          .setStartColumn(
              styleSheet->startPositionInSource().m_column.zeroBasedInt())
          .build();

  if (hasSourceURL())
    result->setHasSourceURL(true);

  if (styleSheet->ownerNode())
    result->setOwnerNode(DOMNodeIds::idForNode(styleSheet->ownerNode()));

  String sourceMapURLValue = sourceMapURL();
  if (!sourceMapURLValue.isEmpty())
    result->setSourceMapURL(sourceMapURLValue);
  return result;
}

std::unique_ptr<protocol::Array<protocol::CSS::Value>>
InspectorStyleSheet::selectorsFromSource(CSSRuleSourceData* sourceData,
                                         const String& sheetText) {
  ScriptRegexp comment("/\\*[^]*?\\*/", TextCaseSensitive, MultilineEnabled);
  std::unique_ptr<protocol::Array<protocol::CSS::Value>> result =
      protocol::Array<protocol::CSS::Value>::create();
  const SelectorRangeList& ranges = sourceData->selectorRanges;
  for (size_t i = 0, size = ranges.size(); i < size; ++i) {
    const SourceRange& range = ranges.at(i);
    String selector = sheetText.substring(range.start, range.length());

    // We don't want to see any comments in the selector components, only the
    // meaningful parts.
    int matchLength;
    int offset = 0;
    while ((offset = comment.match(selector, offset, &matchLength)) >= 0)
      selector.replace(offset, matchLength, "");

    std::unique_ptr<protocol::CSS::Value> simpleSelector =
        protocol::CSS::Value::create()
            .setText(selector.stripWhiteSpace())
            .build();
    simpleSelector->setRange(buildSourceRangeObject(range));
    result->addItem(std::move(simpleSelector));
  }
  return result;
}

std::unique_ptr<protocol::CSS::SelectorList>
InspectorStyleSheet::buildObjectForSelectorList(CSSStyleRule* rule) {
  CSSRuleSourceData* sourceData = sourceDataForRule(rule);
  std::unique_ptr<protocol::Array<protocol::CSS::Value>> selectors;

  // This intentionally does not rely on the source data to avoid catching the
  // trailing comments (before the declaration starting '{').
  String selectorText = rule->selectorText();

  if (sourceData) {
    selectors = selectorsFromSource(sourceData, m_text);
  } else {
    selectors = protocol::Array<protocol::CSS::Value>::create();
    const CSSSelectorList& selectorList = rule->styleRule()->selectorList();
    for (const CSSSelector* selector = selectorList.first(); selector;
         selector = CSSSelectorList::next(*selector))
      selectors->addItem(protocol::CSS::Value::create()
                             .setText(selector->selectorText())
                             .build());
  }
  return protocol::CSS::SelectorList::create()
      .setSelectors(std::move(selectors))
      .setText(selectorText)
      .build();
}

static bool canBind(const String& origin) {
  return origin != protocol::CSS::StyleSheetOriginEnum::UserAgent &&
         origin != protocol::CSS::StyleSheetOriginEnum::Injected;
}

std::unique_ptr<protocol::CSS::CSSRule>
InspectorStyleSheet::buildObjectForRuleWithoutMedia(CSSStyleRule* rule) {
  CSSStyleSheet* styleSheet = pageStyleSheet();
  if (!styleSheet)
    return nullptr;

  std::unique_ptr<protocol::CSS::CSSRule> result =
      protocol::CSS::CSSRule::create()
          .setSelectorList(buildObjectForSelectorList(rule))
          .setOrigin(m_origin)
          .setStyle(buildObjectForStyle(rule->style()))
          .build();

  if (canBind(m_origin)) {
    if (!id().isEmpty())
      result->setStyleSheetId(id());
  }

  return result;
}

std::unique_ptr<protocol::CSS::RuleUsage>
InspectorStyleSheet::buildObjectForRuleUsage(CSSRule* rule, bool wasUsed) {
  CSSStyleSheet* styleSheet = pageStyleSheet();
  if (!styleSheet)
    return nullptr;

  CSSRuleSourceData* sourceData = sourceDataForRule(rule);

  if (!sourceData)
    return nullptr;

  std::unique_ptr<protocol::CSS::RuleUsage> result =
      protocol::CSS::RuleUsage::create()
          .setStyleSheetId(id())
          .setRange(buildSourceRangeObject(sourceData->ruleBodyRange))
          .setUsed(wasUsed)
          .build();

  return result;
}

std::unique_ptr<protocol::CSS::CSSKeyframeRule>
InspectorStyleSheet::buildObjectForKeyframeRule(CSSKeyframeRule* keyframeRule) {
  CSSStyleSheet* styleSheet = pageStyleSheet();
  if (!styleSheet)
    return nullptr;

  std::unique_ptr<protocol::CSS::Value> keyText =
      protocol::CSS::Value::create().setText(keyframeRule->keyText()).build();
  CSSRuleSourceData* sourceData = sourceDataForRule(keyframeRule);
  if (sourceData)
    keyText->setRange(buildSourceRangeObject(sourceData->ruleHeaderRange));
  std::unique_ptr<protocol::CSS::CSSKeyframeRule> result =
      protocol::CSS::CSSKeyframeRule::create()
          // TODO(samli): keyText() normalises 'from' and 'to' keyword values.
          .setKeyText(std::move(keyText))
          .setOrigin(m_origin)
          .setStyle(buildObjectForStyle(keyframeRule->style()))
          .build();
  if (canBind(m_origin) && !id().isEmpty())
    result->setStyleSheetId(id());
  return result;
}

bool InspectorStyleSheet::getText(String* result) {
  if (m_sourceData) {
    *result = m_text;
    return true;
  }
  return false;
}

std::unique_ptr<protocol::CSS::SourceRange>
InspectorStyleSheet::ruleHeaderSourceRange(CSSRule* rule) {
  if (!m_sourceData)
    return nullptr;
  CSSRuleSourceData* sourceData = sourceDataForRule(rule);
  if (!sourceData)
    return nullptr;
  return buildSourceRangeObject(sourceData->ruleHeaderRange);
}

std::unique_ptr<protocol::CSS::SourceRange>
InspectorStyleSheet::mediaQueryExpValueSourceRange(CSSRule* rule,
                                                   size_t mediaQueryIndex,
                                                   size_t mediaQueryExpIndex) {
  if (!m_sourceData)
    return nullptr;
  CSSRuleSourceData* sourceData = sourceDataForRule(rule);
  if (!sourceData || !sourceData->mediaSourceData ||
      mediaQueryIndex >= sourceData->mediaSourceData->queryData.size())
    return nullptr;
  CSSMediaQuerySourceData* mediaQueryData =
      sourceData->mediaSourceData->queryData.at(mediaQueryIndex).get();
  if (mediaQueryExpIndex >= mediaQueryData->expData.size())
    return nullptr;
  return buildSourceRangeObject(
      mediaQueryData->expData.at(mediaQueryExpIndex).valueRange);
}

InspectorStyle* InspectorStyleSheet::inspectorStyle(
    CSSStyleDeclaration* style) {
  return style ? InspectorStyle::create(
                     style, sourceDataForRule(style->parentRule()), this)
               : nullptr;
}

String InspectorStyleSheet::sourceURL() {
  if (!m_sourceURL.isNull())
    return m_sourceURL;
  if (m_origin != protocol::CSS::StyleSheetOriginEnum::Regular) {
    m_sourceURL = "";
    return m_sourceURL;
  }

  String styleSheetText;
  bool success = getText(&styleSheetText);
  if (success) {
    String commentValue = findMagicComment(styleSheetText, "sourceURL");
    if (!commentValue.isEmpty()) {
      m_sourceURL = commentValue;
      return commentValue;
    }
  }
  m_sourceURL = "";
  return m_sourceURL;
}

String InspectorStyleSheet::url() {
  // "sourceURL" is present only for regular rules, otherwise "origin" should be
  // used in the frontend.
  if (m_origin != protocol::CSS::StyleSheetOriginEnum::Regular)
    return String();

  CSSStyleSheet* styleSheet = pageStyleSheet();
  if (!styleSheet)
    return String();

  if (hasSourceURL())
    return sourceURL();

  if (styleSheet->isInline() && startsAtZero())
    return String();

  return finalURL();
}

bool InspectorStyleSheet::hasSourceURL() {
  return !sourceURL().isEmpty();
}

bool InspectorStyleSheet::startsAtZero() {
  CSSStyleSheet* styleSheet = pageStyleSheet();
  if (!styleSheet)
    return true;

  return styleSheet->startPositionInSource() == TextPosition::minimumPosition();
}

String InspectorStyleSheet::sourceMapURL() {
  if (m_origin != protocol::CSS::StyleSheetOriginEnum::Regular)
    return String();

  String styleSheetText;
  bool success = getText(&styleSheetText);
  if (success) {
    String commentValue = findMagicComment(styleSheetText, "sourceMappingURL");
    if (!commentValue.isEmpty())
      return commentValue;
  }
  return m_pageStyleSheet->contents()->sourceMapURL();
}

CSSRuleSourceData* InspectorStyleSheet::findRuleByHeaderRange(
    const SourceRange& sourceRange) {
  if (!m_sourceData)
    return nullptr;

  for (size_t i = 0; i < m_sourceData->size(); ++i) {
    CSSRuleSourceData* ruleSourceData = m_sourceData->at(i).get();
    if (ruleSourceData->ruleHeaderRange.start == sourceRange.start &&
        ruleSourceData->ruleHeaderRange.end == sourceRange.end) {
      return ruleSourceData;
    }
  }
  return nullptr;
}

CSSRuleSourceData* InspectorStyleSheet::findRuleByBodyRange(
    const SourceRange& sourceRange) {
  if (!m_sourceData)
    return nullptr;

  for (size_t i = 0; i < m_sourceData->size(); ++i) {
    CSSRuleSourceData* ruleSourceData = m_sourceData->at(i).get();
    if (ruleSourceData->ruleBodyRange.start == sourceRange.start &&
        ruleSourceData->ruleBodyRange.end == sourceRange.end) {
      return ruleSourceData;
    }
  }
  return nullptr;
}

CSSRule* InspectorStyleSheet::ruleForSourceData(CSSRuleSourceData* sourceData) {
  if (!m_sourceData || !sourceData)
    return nullptr;

  remapSourceDataToCSSOMIfNecessary();

  size_t index = m_sourceData->find(sourceData);
  if (index == kNotFound)
    return nullptr;
  IndexMap::iterator it = m_sourceDataToRule.find(index);
  if (it == m_sourceDataToRule.end())
    return nullptr;

  ASSERT(it->value < m_cssomFlatRules.size());

  // Check that CSSOM did not mutate this rule.
  CSSRule* result = m_cssomFlatRules.at(it->value);
  if (canonicalCSSText(m_parsedFlatRules.at(index)) != canonicalCSSText(result))
    return nullptr;
  return result;
}

CSSRuleSourceData* InspectorStyleSheet::sourceDataForRule(CSSRule* rule) {
  if (!m_sourceData || !rule)
    return nullptr;

  remapSourceDataToCSSOMIfNecessary();

  size_t index = m_cssomFlatRules.find(rule);
  if (index == kNotFound)
    return nullptr;
  IndexMap::iterator it = m_ruleToSourceData.find(index);
  if (it == m_ruleToSourceData.end())
    return nullptr;

  ASSERT(it->value < m_sourceData->size());

  // Check that CSSOM did not mutate this rule.
  CSSRule* parsedRule = m_parsedFlatRules.at(it->value);
  if (canonicalCSSText(rule) != canonicalCSSText(parsedRule))
    return nullptr;

  return m_sourceData->at(it->value).get();
}

void InspectorStyleSheet::remapSourceDataToCSSOMIfNecessary() {
  CSSRuleVector cssomRules;
  collectFlatRules(m_pageStyleSheet.get(), &cssomRules);

  if (cssomRules.size() != m_cssomFlatRules.size()) {
    mapSourceDataToCSSOM();
    return;
  }

  for (size_t i = 0; i < m_cssomFlatRules.size(); ++i) {
    if (m_cssomFlatRules.at(i) != cssomRules.at(i)) {
      mapSourceDataToCSSOM();
      return;
    }
  }
}

void InspectorStyleSheet::mapSourceDataToCSSOM() {
  m_ruleToSourceData.clear();
  m_sourceDataToRule.clear();

  m_cssomFlatRules.clear();
  CSSRuleVector& cssomRules = m_cssomFlatRules;
  collectFlatRules(m_pageStyleSheet.get(), &cssomRules);

  if (!m_sourceData)
    return;

  CSSRuleVector& parsedRules = m_parsedFlatRules;

  Vector<String> cssomRulesText = Vector<String>();
  Vector<String> parsedRulesText = Vector<String>();
  for (size_t i = 0; i < cssomRules.size(); ++i)
    cssomRulesText.push_back(canonicalCSSText(cssomRules.at(i)));
  for (size_t j = 0; j < parsedRules.size(); ++j)
    parsedRulesText.push_back(canonicalCSSText(parsedRules.at(j)));

  diff(cssomRulesText, parsedRulesText, &m_ruleToSourceData,
       &m_sourceDataToRule);
}

const CSSRuleVector& InspectorStyleSheet::flatRules() {
  remapSourceDataToCSSOMIfNecessary();
  return m_cssomFlatRules;
}

bool InspectorStyleSheet::resourceStyleSheetText(String* result) {
  if (m_origin == protocol::CSS::StyleSheetOriginEnum::Injected ||
      m_origin == protocol::CSS::StyleSheetOriginEnum::UserAgent)
    return false;

  if (!m_pageStyleSheet->ownerDocument())
    return false;

  KURL url(ParsedURLString, m_pageStyleSheet->href());
  if (m_resourceContainer->loadStyleSheetContent(url, result))
    return true;

  bool base64Encoded;
  bool success = m_networkAgent->fetchResourceContent(
      m_pageStyleSheet->ownerDocument(), url, result, &base64Encoded);
  return success && !base64Encoded;
}

Element* InspectorStyleSheet::ownerStyleElement() {
  Node* ownerNode = m_pageStyleSheet->ownerNode();
  if (!ownerNode || !ownerNode->isElementNode())
    return nullptr;
  Element* ownerElement = toElement(ownerNode);

  if (!isHTMLStyleElement(ownerElement) && !isSVGStyleElement(ownerElement))
    return nullptr;
  return ownerElement;
}

bool InspectorStyleSheet::inlineStyleSheetText(String* result) {
  Element* ownerElement = ownerStyleElement();
  if (!ownerElement)
    return false;
  if (m_resourceContainer->loadStyleElementContent(
          DOMNodeIds::idForNode(ownerElement), result))
    return true;
  *result = ownerElement->textContent();
  return true;
}

bool InspectorStyleSheet::inspectorStyleSheetText(String* result) {
  if (m_origin != protocol::CSS::StyleSheetOriginEnum::Inspector)
    return false;
  if (!m_pageStyleSheet->ownerDocument())
    return false;
  if (m_resourceContainer->loadStyleElementContent(
          DOMNodeIds::idForNode(m_pageStyleSheet->ownerDocument()), result))
    return true;
  *result = "";
  return true;
}

InspectorStyleSheetForInlineStyle* InspectorStyleSheetForInlineStyle::create(
    Element* element,
    Listener* listener) {
  return new InspectorStyleSheetForInlineStyle(element, listener);
}

InspectorStyleSheetForInlineStyle::InspectorStyleSheetForInlineStyle(
    Element* element,
    Listener* listener)
    : InspectorStyleSheetBase(listener), m_element(element) {
  ASSERT(m_element);
}

void InspectorStyleSheetForInlineStyle::didModifyElementAttribute() {
  m_inspectorStyle.clear();
}

bool InspectorStyleSheetForInlineStyle::setText(
    const String& text,
    ExceptionState& exceptionState) {
  if (!verifyStyleText(&m_element->document(), text)) {
    exceptionState.throwDOMException(SyntaxError, "Style text is not valid.");
    return false;
  }

  {
    InspectorCSSAgent::InlineStyleOverrideScope overrideScope(
        m_element->ownerDocument());
    m_element->setAttribute("style", AtomicString(text), exceptionState);
  }
  if (!exceptionState.hadException())
    onStyleSheetTextChanged();
  return !exceptionState.hadException();
}

bool InspectorStyleSheetForInlineStyle::getText(String* result) {
  *result = elementStyleText();
  return true;
}

InspectorStyle* InspectorStyleSheetForInlineStyle::inspectorStyle(
    CSSStyleDeclaration* style) {
  if (!m_inspectorStyle)
    m_inspectorStyle =
        InspectorStyle::create(m_element->style(), ruleSourceData(), this);

  return m_inspectorStyle;
}

PassRefPtr<CSSRuleSourceData>
InspectorStyleSheetForInlineStyle::ruleSourceData() {
  const String& text = elementStyleText();
  RefPtr<CSSRuleSourceData> ruleSourceData;
  if (text.isEmpty()) {
    ruleSourceData = CSSRuleSourceData::create(StyleRule::Style);
    ruleSourceData->ruleBodyRange.start = 0;
    ruleSourceData->ruleBodyRange.end = 0;
  } else {
    RuleSourceDataList ruleSourceDataResult;
    StyleSheetHandler handler(text, &m_element->document(),
                              &ruleSourceDataResult);
    CSSParser::parseDeclarationListForInspector(
        parserContextForDocument(&m_element->document()), text, handler);
    ruleSourceData = ruleSourceDataResult.front().release();
  }
  return ruleSourceData.release();
}

CSSStyleDeclaration* InspectorStyleSheetForInlineStyle::inlineStyle() {
  return m_element->style();
}

const String& InspectorStyleSheetForInlineStyle::elementStyleText() {
  return m_element->getAttribute("style").getString();
}

DEFINE_TRACE(InspectorStyleSheetForInlineStyle) {
  visitor->trace(m_element);
  visitor->trace(m_inspectorStyle);
  InspectorStyleSheetBase::trace(visitor);
}

}  // namespace blink
