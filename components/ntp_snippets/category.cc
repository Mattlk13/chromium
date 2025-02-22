// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/category.h"

#include "base/logging.h"

namespace ntp_snippets {

// static
Category Category::FromKnownCategory(KnownCategories known_category) {
  return FromIDValue(static_cast<int>(known_category));
}

// static
Category Category::FromRemoteCategory(int remote_category) {
  DCHECK_GT(remote_category, 0);
  return Category(static_cast<int>(KnownCategories::REMOTE_CATEGORIES_OFFSET) +
                  remote_category);
}

// static
Category Category::FromIDValue(int id) {
  DCHECK_GE(id, 0);
  DCHECK(id < static_cast<int>(KnownCategories::LOCAL_CATEGORIES_COUNT) ||
         id > static_cast<int>(KnownCategories::REMOTE_CATEGORIES_OFFSET));
  return Category(id);
}

Category::Category(int id) : id_(id) {}

bool Category::IsKnownCategory(KnownCategories known_category) const {
  DCHECK_NE(known_category, KnownCategories::LOCAL_CATEGORIES_COUNT);
  DCHECK_NE(known_category, KnownCategories::REMOTE_CATEGORIES_OFFSET);
  return id_ == static_cast<int>(known_category);
}

bool operator==(const Category& left, const Category& right) {
  return left.id() == right.id();
}

bool operator!=(const Category& left, const Category& right) {
  return !(left == right);
}

bool Category::CompareByID::operator()(const Category& left,
                                       const Category& right) const {
  return left.id() < right.id();
}

std::ostream& operator<<(std::ostream& os, const Category& obj) {
  os << obj.id();
  return os;
}

}  // namespace ntp_snippets
