// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/golden_tests/metal_screenshot.h"

namespace impeller {
namespace testing {

MetalScreenshot::MetalScreenshot(CGImageRef cgImage) : cgImage_(cgImage) {
  CGDataProviderRef data_provider = CGImageGetDataProvider(cgImage);
  pixel_data_ = CGDataProviderCopyData(data_provider);
}

MetalScreenshot::~MetalScreenshot() {
  CFRelease(pixel_data_);
  CGImageRelease(cgImage_);
}

const UInt8* MetalScreenshot::GetBytes() const {
  return CFDataGetBytePtr(pixel_data_);
}

size_t MetalScreenshot::GetHeight() const {
  return CGImageGetHeight(cgImage_);
}

size_t MetalScreenshot::GetWidth() const {
  return CGImageGetWidth(cgImage_);
}

bool MetalScreenshot::WriteToPNG(const std::string& path) const {
  bool result = false;
  NSURL* output_url =
      [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]];
  CGImageDestinationRef destination = CGImageDestinationCreateWithURL(
      (__bridge CFURLRef)output_url, kUTTypePNG, 1, nullptr);
  if (destination != nullptr) {
    CGImageDestinationAddImage(destination, cgImage_,
                               (__bridge CFDictionaryRef) @{});

    if (CGImageDestinationFinalize(destination)) {
      result = true;
    }

    CFRelease(destination);
  }
  return result;
}

}  // namespace testing
}  // namespace impeller
