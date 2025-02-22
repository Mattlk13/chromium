/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/layout/TextRunConstructor.h"

#include "core/layout/LayoutText.h"
#include "core/layout/api/LineLayoutItem.h"
#include "core/layout/api/LineLayoutText.h"
#include "platform/text/BidiTextRun.h"

namespace blink {

template <typename CharacterType>
static inline TextRun constructTextRunInternal(const Font& font,
                                               const CharacterType* characters,
                                               int length,
                                               const ComputedStyle& style,
                                               TextDirection direction) {
  TextRun::ExpansionBehavior expansion =
      TextRun::AllowTrailingExpansion | TextRun::ForbidLeadingExpansion;
  bool directionalOverride = style.rtlOrdering() == EOrder::Visual;
  TextRun run(characters, length, 0, 0, expansion, direction,
              directionalOverride);
  return run;
}

template <typename CharacterType>
static inline TextRun constructTextRunInternal(const Font& font,
                                               const CharacterType* characters,
                                               int length,
                                               const ComputedStyle& style,
                                               TextDirection direction,
                                               TextRunFlags flags) {
  TextDirection textDirection = direction;
  bool directionalOverride = style.rtlOrdering() == EOrder::Visual;
  if (flags != DefaultTextRunFlags) {
    if (flags & RespectDirection)
      textDirection = style.direction();
    if (flags & RespectDirectionOverride)
      directionalOverride |= isOverride(style.unicodeBidi());
  }

  TextRun::ExpansionBehavior expansion =
      TextRun::AllowTrailingExpansion | TextRun::ForbidLeadingExpansion;
  TextRun run(characters, length, 0, 0, expansion, textDirection,
              directionalOverride);
  return run;
}

TextRun constructTextRun(const Font& font,
                         const LChar* characters,
                         int length,
                         const ComputedStyle& style,
                         TextDirection direction) {
  return constructTextRunInternal(font, characters, length, style, direction);
}

TextRun constructTextRun(const Font& font,
                         const UChar* characters,
                         int length,
                         const ComputedStyle& style,
                         TextDirection direction) {
  return constructTextRunInternal(font, characters, length, style, direction);
}

TextRun constructTextRun(const Font& font,
                         const LayoutText* text,
                         unsigned offset,
                         unsigned length,
                         const ComputedStyle& style,
                         TextDirection direction) {
  ASSERT(offset + length <= text->textLength());
  if (text->hasEmptyText())
    return constructTextRunInternal(font, static_cast<const LChar*>(nullptr), 0,
                                    style, direction);
  if (text->is8Bit())
    return constructTextRunInternal(font, text->characters8() + offset, length,
                                    style, direction);
  return constructTextRunInternal(font, text->characters16() + offset, length,
                                  style, direction);
}

TextRun constructTextRun(const Font& font,
                         const String& string,
                         const ComputedStyle& style,
                         TextDirection direction,
                         TextRunFlags flags) {
  unsigned length = string.length();
  if (!length)
    return constructTextRunInternal(font, static_cast<const LChar*>(nullptr),
                                    length, style, direction, flags);
  if (string.is8Bit())
    return constructTextRunInternal(font, string.characters8(), length, style,
                                    direction, flags);
  return constructTextRunInternal(font, string.characters16(), length, style,
                                  direction, flags);
}

TextRun constructTextRun(const Font& font,
                         const String& string,
                         const ComputedStyle& style,
                         TextRunFlags flags) {
  return constructTextRun(font, string, style,
                          string.isEmpty() || string.is8Bit()
                              ? TextDirection::kLtr
                              : determineDirectionality(string),
                          flags);
}

TextRun constructTextRun(const Font& font,
                         const LineLayoutText text,
                         unsigned offset,
                         unsigned length,
                         const ComputedStyle& style) {
  ASSERT(offset + length <= text.textLength());
  if (text.hasEmptyText()) {
    return constructTextRunInternal(font, static_cast<const LChar*>(nullptr), 0,
                                    style, TextDirection::kLtr);
  }
  if (text.is8Bit()) {
    return constructTextRunInternal(font, text.characters8() + offset, length,
                                    style, TextDirection::kLtr);
  }

  TextRun run = constructTextRunInternal(font, text.characters16() + offset,
                                         length, style, TextDirection::kLtr);
  run.setDirection(directionForRun(run));
  return run;
}

}  // namespace blink
