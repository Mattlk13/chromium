/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 2004-2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2006, 2007 Nicholas Shanks (webkit@nickshanks.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 * Copyright (C) 2007, 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/css/CSSDefaultStyleSheets.h"

#include "core/MathMLNames.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/RuleSet.h"
#include "core/css/StyleSheetContents.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/layout/LayoutTheme.h"
#include "platform/PlatformResourceLoader.h"
#include "wtf/LeakAnnotations.h"

namespace blink {

using namespace HTMLNames;

CSSDefaultStyleSheets& CSSDefaultStyleSheets::instance() {
  DEFINE_STATIC_LOCAL(CSSDefaultStyleSheets, cssDefaultStyleSheets,
                      (new CSSDefaultStyleSheets));
  return cssDefaultStyleSheets;
}

static const MediaQueryEvaluator& screenEval() {
  DEFINE_STATIC_LOCAL(MediaQueryEvaluator, staticScreenEval,
                      (new MediaQueryEvaluator("screen")));
  return staticScreenEval;
}

static const MediaQueryEvaluator& printEval() {
  DEFINE_STATIC_LOCAL(MediaQueryEvaluator, staticPrintEval,
                      (new MediaQueryEvaluator("print")));
  return staticPrintEval;
}

static StyleSheetContents* parseUASheet(const String& str) {
  StyleSheetContents* sheet =
      StyleSheetContents::create(CSSParserContext(UASheetMode, nullptr));
  sheet->parseString(str);
  // User Agent stylesheets are parsed once for the lifetime of the renderer
  // process and are intentionally leaked.
  LEAK_SANITIZER_IGNORE_OBJECT(sheet);
  return sheet;
}

CSSDefaultStyleSheets::CSSDefaultStyleSheets() {
  m_defaultStyle = RuleSet::create();
  m_defaultPrintStyle = RuleSet::create();
  m_defaultQuirksStyle = RuleSet::create();

  // Strict-mode rules.
  String defaultRules = loadResourceAsASCIIString("html.css") +
                        LayoutTheme::theme().extraDefaultStyleSheet();
  m_defaultStyleSheet = parseUASheet(defaultRules);
  m_defaultStyle->addRulesFromSheet(defaultStyleSheet(), screenEval());
  m_defaultPrintStyle->addRulesFromSheet(defaultStyleSheet(), printEval());

  // Quirks-mode rules.
  String quirksRules = loadResourceAsASCIIString("quirks.css") +
                       LayoutTheme::theme().extraQuirksStyleSheet();
  m_quirksStyleSheet = parseUASheet(quirksRules);
  m_defaultQuirksStyle->addRulesFromSheet(quirksStyleSheet(), screenEval());
}

RuleSet* CSSDefaultStyleSheets::defaultViewSourceStyle() {
  if (!m_defaultViewSourceStyle) {
    m_defaultViewSourceStyle = RuleSet::create();
    // Loaded stylesheet is leaked on purpose.
    StyleSheetContents* stylesheet =
        parseUASheet(loadResourceAsASCIIString("view-source.css"));
    m_defaultViewSourceStyle->addRulesFromSheet(stylesheet, screenEval());
  }
  return m_defaultViewSourceStyle;
}

StyleSheetContents*
CSSDefaultStyleSheets::ensureXHTMLMobileProfileStyleSheet() {
  if (!m_xhtmlMobileProfileStyleSheet)
    m_xhtmlMobileProfileStyleSheet =
        parseUASheet(loadResourceAsASCIIString("xhtmlmp.css"));
  return m_xhtmlMobileProfileStyleSheet;
}

StyleSheetContents* CSSDefaultStyleSheets::ensureMobileViewportStyleSheet() {
  if (!m_mobileViewportStyleSheet)
    m_mobileViewportStyleSheet =
        parseUASheet(loadResourceAsASCIIString("viewportAndroid.css"));
  return m_mobileViewportStyleSheet;
}

StyleSheetContents*
CSSDefaultStyleSheets::ensureTelevisionViewportStyleSheet() {
  if (!m_televisionViewportStyleSheet)
    m_televisionViewportStyleSheet =
        parseUASheet(loadResourceAsASCIIString("viewportTelevision.css"));
  return m_televisionViewportStyleSheet;
}

bool CSSDefaultStyleSheets::ensureDefaultStyleSheetsForElement(
    const Element& element) {
  bool changedDefaultStyle = false;
  // FIXME: We should assert that the sheet only styles SVG elements.
  if (element.isSVGElement() && !m_svgStyleSheet) {
    m_svgStyleSheet = parseUASheet(loadResourceAsASCIIString("svg.css"));
    m_defaultStyle->addRulesFromSheet(svgStyleSheet(), screenEval());
    m_defaultPrintStyle->addRulesFromSheet(svgStyleSheet(), printEval());
    changedDefaultStyle = true;
  }

  // FIXME: We should assert that the sheet only styles MathML elements.
  if (element.namespaceURI() == MathMLNames::mathmlNamespaceURI &&
      !m_mathmlStyleSheet) {
    m_mathmlStyleSheet = parseUASheet(loadResourceAsASCIIString("mathml.css"));
    m_defaultStyle->addRulesFromSheet(mathmlStyleSheet(), screenEval());
    m_defaultPrintStyle->addRulesFromSheet(mathmlStyleSheet(), printEval());
    changedDefaultStyle = true;
  }

  // FIXME: We should assert that this sheet only contains rules for <video> and
  // <audio>.
  if (!m_mediaControlsStyleSheet &&
      (isHTMLVideoElement(element) || isHTMLAudioElement(element))) {
    String mediaRules = loadResourceAsASCIIString("mediaControls.css") +
                        LayoutTheme::theme().extraMediaControlsStyleSheet();
    m_mediaControlsStyleSheet = parseUASheet(mediaRules);
    m_defaultStyle->addRulesFromSheet(mediaControlsStyleSheet(), screenEval());
    m_defaultPrintStyle->addRulesFromSheet(mediaControlsStyleSheet(),
                                           printEval());
    changedDefaultStyle = true;
  }

  DCHECK(!m_defaultStyle->features().hasIdsInSelectors());
  DCHECK(!m_defaultStyle->features().usesSiblingRules());
  return changedDefaultStyle;
}

void CSSDefaultStyleSheets::ensureDefaultStyleSheetForFullscreen() {
  if (m_fullscreenStyleSheet)
    return;

  String fullscreenRules = loadResourceAsASCIIString("fullscreen.css") +
                           LayoutTheme::theme().extraFullscreenStyleSheet();
  m_fullscreenStyleSheet = parseUASheet(fullscreenRules);
  m_defaultStyle->addRulesFromSheet(fullscreenStyleSheet(), screenEval());
  m_defaultQuirksStyle->addRulesFromSheet(fullscreenStyleSheet(), screenEval());
}

DEFINE_TRACE(CSSDefaultStyleSheets) {
  visitor->trace(m_defaultStyle);
  visitor->trace(m_defaultQuirksStyle);
  visitor->trace(m_defaultPrintStyle);
  visitor->trace(m_defaultViewSourceStyle);
  visitor->trace(m_defaultStyleSheet);
  visitor->trace(m_mobileViewportStyleSheet);
  visitor->trace(m_televisionViewportStyleSheet);
  visitor->trace(m_xhtmlMobileProfileStyleSheet);
  visitor->trace(m_quirksStyleSheet);
  visitor->trace(m_svgStyleSheet);
  visitor->trace(m_mathmlStyleSheet);
  visitor->trace(m_mediaControlsStyleSheet);
  visitor->trace(m_fullscreenStyleSheet);
}

}  // namespace blink
