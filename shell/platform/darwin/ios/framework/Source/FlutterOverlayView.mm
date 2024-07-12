// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "flutter/shell/platform/darwin/ios/framework/Source/FlutterOverlayView.h"

#include "flutter/common/settings.h"
#include "flutter/common/task_runners.h"
#include "flutter/flow/layers/layer_tree.h"
#include "flutter/fml/platform/darwin/cf_utils.h"
#include "flutter/fml/synchronization/waitable_event.h"
#include "flutter/fml/trace_event.h"
#include "flutter/shell/common/platform_view.h"
#include "flutter/shell/common/rasterizer.h"
#import "flutter/shell/platform/darwin/ios/framework/Source/FlutterView.h"
#import "flutter/shell/platform/darwin/ios/ios_surface_software.h"
#include "third_party/skia/include/utils/mac/SkCGUtils.h"

// This is mostly a duplication of FlutterView.
// TODO(amirh): once GL support is in evaluate if we can merge this with FlutterView.
@implementation FlutterOverlayView

- (instancetype)initWithFrame:(CGRect)frame {
  NSAssert(NO, @"FlutterOverlayView must init or initWithContentsScale");
  return nil;
}

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  NSAssert(NO, @"FlutterOverlayView must init or initWithContentsScale");
  return nil;
}

- (instancetype)init {
  self = [super initWithFrame:CGRectZero];

  if (self) {
    self.layer.opaque = NO;
    self.userInteractionEnabled = NO;
    self.autoresizingMask = (UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight);
  }

  return self;
}

- (instancetype)initWithContentsScale:(CGFloat)contentsScale {
  self = [self init];

  if ([self.layer isKindOfClass:NSClassFromString(@"CAMetalLayer")]) {
    self.layer.allowsGroupOpacity = NO;
    self.layer.contentsScale = contentsScale;
    self.layer.rasterizationScale = contentsScale;
  }

  return self;
}

+ (Class)layerClass {
  return [FlutterView layerClass];
}

// TODO(amirh): implement drawLayer to support snapshotting.

@end
