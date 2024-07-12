// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "flutter/display_list/image/dl_image.h"
#include "flutter/fml/macros.h"
#include "impeller/core/texture.h"

namespace impeller {

class AiksContext;

class DlImageImpeller final : public flutter::DlImage {
 public:
  static sk_sp<DlImageImpeller> Make(
      std::shared_ptr<Texture> texture,
      OwningContext owning_context = OwningContext::kIO);

  static sk_sp<DlImageImpeller> MakeFromYUVTextures(
      AiksContext* aiks_context,
      std::shared_ptr<Texture> y_texture,
      std::shared_ptr<Texture> uv_texture,
      YUVColorSpace yuv_color_space);

  // |DlImage|
  ~DlImageImpeller() override;

  // |DlImage|
  sk_sp<SkImage> skia_image() const override;

  // |DlImage|
  std::shared_ptr<impeller::Texture> impeller_texture() const override;

  // |DlImage|
  bool isOpaque() const override;

  // |DlImage|
  bool isTextureBacked() const override;

  // |DlImage|
  SkISize dimensions() const override;

  // |DlImage|
  size_t GetApproximateByteSize() const override;

  // |DlImage|
  OwningContext owning_context() const override { return owning_context_; }

 private:
  std::shared_ptr<Texture> texture_;
  OwningContext owning_context_;

  explicit DlImageImpeller(std::shared_ptr<Texture> texture,
                           OwningContext owning_context = OwningContext::kIO);

  FML_DISALLOW_COPY_AND_ASSIGN(DlImageImpeller);
};

}  // namespace impeller
