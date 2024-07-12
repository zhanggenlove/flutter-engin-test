// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/flow/layers/container_layer.h"
#include "flutter/flow/layers/display_list_layer.h"
#include "flutter/flow/layers/opacity_layer.h"
#include "flutter/flow/testing/skia_gpu_object_layer_test.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace flutter {
namespace testing {

class MockLayerTree {
 public:
  explicit MockLayerTree(SkISize size = SkISize::Make(1000, 1000))
      : root_(std::make_shared<ContainerLayer>()), size_(size) {}

  ContainerLayer* root() { return root_.get(); }
  const ContainerLayer* root() const { return root_.get(); }

  PaintRegionMap& paint_region_map() { return paint_region_map_; }
  const PaintRegionMap& paint_region_map() const { return paint_region_map_; }

  const SkISize& size() const { return size_; }

 private:
  std::shared_ptr<ContainerLayer> root_;
  PaintRegionMap paint_region_map_;
  SkISize size_;
};

class DiffContextTest : public ThreadTest {
 public:
  DiffContextTest();

  Damage DiffLayerTree(MockLayerTree& layer_tree,
                       const MockLayerTree& old_layer_tree,
                       const SkIRect& additional_damage = SkIRect::MakeEmpty(),
                       int horizontal_clip_alignment = 0,
                       int vertical_alignment = 0,
                       bool use_raster_cache = true);

  // Create display list consisting of filled rect with given color; Being able
  // to specify different color is useful to test deep comparison of pictures
  sk_sp<DisplayList> CreateDisplayList(const SkRect& bounds, uint32_t color);

  std::shared_ptr<DisplayListLayer> CreateDisplayListLayer(
      sk_sp<DisplayList> display_list,
      const SkPoint& offset = SkPoint::Make(0, 0));

  std::shared_ptr<ContainerLayer> CreateContainerLayer(
      std::initializer_list<std::shared_ptr<Layer>> layers);

  std::shared_ptr<ContainerLayer> CreateContainerLayer(
      std::shared_ptr<Layer> l) {
    return CreateContainerLayer({l});
  }

  std::shared_ptr<OpacityLayer> CreateOpacityLater(
      std::initializer_list<std::shared_ptr<Layer>> layers,
      SkAlpha alpha,
      const SkPoint& offset = SkPoint::Make(0, 0));

  fml::RefPtr<SkiaUnrefQueue> unref_queue() { return unref_queue_; }

 private:
  fml::RefPtr<SkiaUnrefQueue> unref_queue_;
};

}  // namespace testing
}  // namespace flutter
