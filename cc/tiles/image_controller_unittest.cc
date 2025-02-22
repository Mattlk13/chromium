// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/image_controller.h"
#include "cc/tiles/image_decode_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class TestableCache : public ImageDecodeCache {
 public:
  bool GetTaskForImageAndRef(const DrawImage& image,
                             const TracingInfo& tracing_info,
                             scoped_refptr<TileTask>* task) override {
    *task = nullptr;
    ++number_of_refs_;
    return true;
  }

  void UnrefImage(const DrawImage& image) override {
    ASSERT_GT(number_of_refs_, 0);
    --number_of_refs_;
  }
  DecodedDrawImage GetDecodedImageForDraw(const DrawImage& image) override {
    return DecodedDrawImage(nullptr, kNone_SkFilterQuality);
  }
  void DrawWithImageFinished(const DrawImage& image,
                             const DecodedDrawImage& decoded_image) override {}
  void ReduceCacheUsage() override {}
  void SetShouldAggressivelyFreeResources(
      bool aggressively_free_resources) override {}

  int number_of_refs() const { return number_of_refs_; }

 private:
  int number_of_refs_ = 0;
};

TEST(ImageControllerTest, NullCacheUnrefsImages) {
  TestableCache cache;
  ImageController controller;
  controller.SetImageDecodeCache(&cache);

  std::vector<DrawImage> images(10);
  ImageDecodeCache::TracingInfo tracing_info;

  ASSERT_EQ(10u, images.size());
  auto tasks = controller.SetPredecodeImages(std::move(images), tracing_info);
  EXPECT_EQ(0u, tasks.size());
  EXPECT_EQ(10, cache.number_of_refs());

  controller.SetImageDecodeCache(nullptr);
  EXPECT_EQ(0, cache.number_of_refs());
}

}  // namespace cc
