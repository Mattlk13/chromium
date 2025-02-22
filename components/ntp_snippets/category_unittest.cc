// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/category.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ntp_snippets {

TEST(CategoryTest, FromKnownCategoryShouldReturnSameCategoryForSameInput) {
  const KnownCategories known_category = KnownCategories::BOOKMARKS;
  Category first = Category::FromKnownCategory(known_category);
  Category second = Category::FromKnownCategory(known_category);
  EXPECT_EQ(first, second);
}

TEST(CategoryFactoryTest,
     FromRemoteCategoryShouldReturnSameCategoryForSameInput) {
  const int remote_category_id = 123;
  Category first = Category::FromRemoteCategory(remote_category_id);
  Category second = Category::FromRemoteCategory(remote_category_id);
  EXPECT_EQ(first, second);
}

TEST(CategoryFactoryTest, FromIDValueShouldReturnSameKnownCategory) {
  Category known_category =
      Category::FromKnownCategory(KnownCategories::BOOKMARKS);
  Category known_category_by_id = Category::FromIDValue(known_category.id());
  EXPECT_EQ(known_category, known_category_by_id);
}

TEST(CategoryFactoryTest, FromIDValueShouldReturnSameRemoteCategory) {
  const int remote_category_id = 123;
  Category remote_category = Category::FromRemoteCategory(remote_category_id);
  Category remote_category_by_id = Category::FromIDValue(remote_category.id());
  EXPECT_EQ(remote_category, remote_category_by_id);
}

}  // namespace ntp_snippets
