// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/properties/CSSPropertyAPITextDecorationColor.h"

#include "core/css/parser/CSSParserContext.h"
#include "core/css/parser/CSSPropertyParserHelpers.h"
#include "platform/RuntimeEnabledFeatures.h"

namespace blink {

const CSSValue* CSSPropertyAPITextDecorationColor::parseSingleValue(
    CSSParserTokenRange& range,
    const CSSParserContext& context) {
  DCHECK(RuntimeEnabledFeatures::css3TextDecorationsEnabled());
  return CSSPropertyParserHelpers::consumeColor(range, context.mode());
}

}  // namespace blink
