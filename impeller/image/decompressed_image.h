// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>
#include <optional>

#include "flutter/fml/macros.h"
#include "flutter/fml/mapping.h"
#include "impeller/geometry/size.h"

namespace impeller {

class DecompressedImage {
 public:
  enum class Format {
    kInvalid,
    kGrey,
    kGreyAlpha,
    kRGB,
    kRGBA,
  };

  DecompressedImage();

  DecompressedImage(ISize size,
                    Format format,
                    std::shared_ptr<const fml::Mapping> allocation);

  ~DecompressedImage();

  const ISize& GetSize() const;

  bool IsValid() const;

  Format GetFormat() const;

  const std::shared_ptr<const fml::Mapping>& GetAllocation() const;

  DecompressedImage ConvertToRGBA() const;

 private:
  ISize size_;
  Format format_ = Format::kInvalid;
  std::shared_ptr<const fml::Mapping> allocation_;
  bool is_valid_ = false;
};

}  // namespace impeller
