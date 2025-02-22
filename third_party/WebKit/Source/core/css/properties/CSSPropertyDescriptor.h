// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/CSSPropertyNames.h"

namespace blink {

class CSSValue;
class CSSParserTokenRange;
class CSSParserContext;

// Stores function pointers matching those declared in CSSPropertyAPI.
struct CSSPropertyDescriptor {
  const CSSValue* (*parseSingleValue)(CSSParserTokenRange&,
                                      const CSSParserContext&);

  // Stores whether or not this descriptor is for a valid property. Do not
  // access the contents of this descriptor unless this value is true.
  // TODO(aazzam): Remove this once the switch in
  // CSSPropertyParser::parseSingleValue() has been completely replaced by
  // CSSPropertyDescriptors.
  bool temporaryCanReadValue;

  // Returns the corresponding CSSPropertyDescriptor for a given CSSPropertyID.
  // Use this function to access the API for a property. Returns a descriptor
  // with isValid set to false if no descriptor exists for this ID.
  static const CSSPropertyDescriptor& get(CSSPropertyID);
};

}  // namespace blink
