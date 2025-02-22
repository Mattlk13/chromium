/*
 * Copyright (C) 2007, 2008, 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/css/CSSKeyframesRule.h"

#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/parser/CSSParser.h"
#include "core/frame/UseCounter.h"
#include "wtf/text/StringBuilder.h"
#include <memory>

namespace blink {

StyleRuleKeyframes::StyleRuleKeyframes()
    : StyleRuleBase(Keyframes), m_version(0) {}

StyleRuleKeyframes::StyleRuleKeyframes(const StyleRuleKeyframes& o)
    : StyleRuleBase(o),
      m_keyframes(o.m_keyframes),
      m_name(o.m_name),
      m_version(o.m_version),
      m_isPrefixed(o.m_isPrefixed) {}

StyleRuleKeyframes::~StyleRuleKeyframes() {}

void StyleRuleKeyframes::parserAppendKeyframe(StyleRuleKeyframe* keyframe) {
  if (!keyframe)
    return;
  m_keyframes.push_back(keyframe);
}

void StyleRuleKeyframes::wrapperAppendKeyframe(StyleRuleKeyframe* keyframe) {
  m_keyframes.push_back(keyframe);
  styleChanged();
}

void StyleRuleKeyframes::wrapperRemoveKeyframe(unsigned index) {
  m_keyframes.remove(index);
  styleChanged();
}

int StyleRuleKeyframes::findKeyframeIndex(const String& key) const {
  std::unique_ptr<Vector<double>> keys = CSSParser::parseKeyframeKeyList(key);
  if (!keys)
    return -1;
  for (size_t i = m_keyframes.size(); i--;) {
    if (m_keyframes[i]->keys() == *keys)
      return i;
  }
  return -1;
}

DEFINE_TRACE_AFTER_DISPATCH(StyleRuleKeyframes) {
  visitor->trace(m_keyframes);
  StyleRuleBase::traceAfterDispatch(visitor);
}

CSSKeyframesRule::CSSKeyframesRule(StyleRuleKeyframes* keyframesRule,
                                   CSSStyleSheet* parent)
    : CSSRule(parent),
      m_keyframesRule(keyframesRule),
      m_childRuleCSSOMWrappers(keyframesRule->keyframes().size()),
      m_isPrefixed(keyframesRule->isVendorPrefixed()) {}

CSSKeyframesRule::~CSSKeyframesRule() {}

void CSSKeyframesRule::setName(const String& name) {
  CSSStyleSheet::RuleMutationScope mutationScope(this);

  m_keyframesRule->setName(name);
}

void CSSKeyframesRule::appendRule(const String& ruleText) {
  ASSERT(m_childRuleCSSOMWrappers.size() ==
         m_keyframesRule->keyframes().size());

  CSSStyleSheet* styleSheet = parentStyleSheet();
  CSSParserContext context(parserContext(), UseCounter::getFrom(styleSheet));
  StyleRuleKeyframe* keyframe = CSSParser::parseKeyframeRule(context, ruleText);
  if (!keyframe)
    return;

  CSSStyleSheet::RuleMutationScope mutationScope(this);

  m_keyframesRule->wrapperAppendKeyframe(keyframe);

  m_childRuleCSSOMWrappers.grow(length());
}

void CSSKeyframesRule::deleteRule(const String& s) {
  ASSERT(m_childRuleCSSOMWrappers.size() ==
         m_keyframesRule->keyframes().size());

  int i = m_keyframesRule->findKeyframeIndex(s);
  if (i < 0)
    return;

  CSSStyleSheet::RuleMutationScope mutationScope(this);

  m_keyframesRule->wrapperRemoveKeyframe(i);

  if (m_childRuleCSSOMWrappers[i])
    m_childRuleCSSOMWrappers[i]->setParentRule(0);
  m_childRuleCSSOMWrappers.remove(i);
}

CSSKeyframeRule* CSSKeyframesRule::findRule(const String& s) {
  int i = m_keyframesRule->findKeyframeIndex(s);
  return (i >= 0) ? item(i) : nullptr;
}

String CSSKeyframesRule::cssText() const {
  StringBuilder result;
  if (isVendorPrefixed())
    result.append("@-webkit-keyframes ");
  else
    result.append("@keyframes ");
  result.append(name());
  result.append(" { \n");

  unsigned size = length();
  for (unsigned i = 0; i < size; ++i) {
    result.append("  ");
    result.append(m_keyframesRule->keyframes()[i]->cssText());
    result.append('\n');
  }
  result.append('}');
  return result.toString();
}

unsigned CSSKeyframesRule::length() const {
  return m_keyframesRule->keyframes().size();
}

CSSKeyframeRule* CSSKeyframesRule::item(unsigned index) const {
  if (index >= length())
    return nullptr;

  ASSERT(m_childRuleCSSOMWrappers.size() ==
         m_keyframesRule->keyframes().size());
  Member<CSSKeyframeRule>& rule = m_childRuleCSSOMWrappers[index];
  if (!rule)
    rule = new CSSKeyframeRule(m_keyframesRule->keyframes()[index].get(),
                               const_cast<CSSKeyframesRule*>(this));

  return rule.get();
}

CSSKeyframeRule* CSSKeyframesRule::anonymousIndexedGetter(
    unsigned index) const {
  if (UseCounter* useCounter = UseCounter::getFrom(parentStyleSheet()))
    useCounter->count(UseCounter::CSSKeyframesRuleAnonymousIndexedGetter);
  return item(index);
}

CSSRuleList* CSSKeyframesRule::cssRules() const {
  if (!m_ruleListCSSOMWrapper)
    m_ruleListCSSOMWrapper = LiveCSSRuleList<CSSKeyframesRule>::create(
        const_cast<CSSKeyframesRule*>(this));
  return m_ruleListCSSOMWrapper.get();
}

void CSSKeyframesRule::reattach(StyleRuleBase* rule) {
  ASSERT(rule);
  m_keyframesRule = toStyleRuleKeyframes(rule);
}

DEFINE_TRACE(CSSKeyframesRule) {
  CSSRule::trace(visitor);
  visitor->trace(m_childRuleCSSOMWrappers);
  visitor->trace(m_keyframesRule);
  visitor->trace(m_ruleListCSSOMWrapper);
}

}  // namespace blink
