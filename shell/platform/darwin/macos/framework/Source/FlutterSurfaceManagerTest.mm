// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <OCMock/OCMock.h>

#import "flutter/shell/platform/darwin/macos/framework/Source/FlutterSurface.h"
#import "flutter/shell/platform/darwin/macos/framework/Source/FlutterSurfaceManager.h"

#include "flutter/testing/testing.h"
#include "gtest/gtest.h"

@interface TestView : NSView <FlutterSurfaceManagerDelegate>

@property(readwrite, nonatomic) CGSize presentedFrameSize;
- (nonnull instancetype)init;

@end

@implementation TestView

- (instancetype)init {
  self = [super initWithFrame:NSZeroRect];
  if (self) {
    [self setWantsLayer:YES];
  }
  return self;
}

- (void)onPresent:(CGSize)frameSize withBlock:(nonnull dispatch_block_t)block {
  self.presentedFrameSize = frameSize;
  block();
}

@end

namespace flutter::testing {

static FlutterSurfaceManager* CreateSurfaceManager(TestView* testView) {
  id<MTLDevice> device = MTLCreateSystemDefaultDevice();
  id<MTLCommandQueue> commandQueue = [device newCommandQueue];
  CALayer* layer = reinterpret_cast<CALayer*>(testView.layer);
  return [[FlutterSurfaceManager alloc] initWithDevice:device
                                          commandQueue:commandQueue
                                                 layer:layer
                                              delegate:testView];
}

static FlutterSurfacePresentInfo* CreatePresentInfo(FlutterSurface* surface,
                                                    CGPoint offset = CGPointZero,
                                                    size_t index = 0) {
  FlutterSurfacePresentInfo* res = [[FlutterSurfacePresentInfo alloc] init];
  res.surface = surface;
  res.offset = offset;
  res.zIndex = index;
  return res;
}

TEST(FlutterSurfaceManager, MetalTextureSizeMatchesSurfaceSize) {
  TestView* testView = [[TestView alloc] init];
  FlutterSurfaceManager* surfaceManager = CreateSurfaceManager(testView);

  // Get back buffer, lookup should work for borrowed surfaces util present.
  auto surface = [surfaceManager surfaceForSize:CGSizeMake(100, 50)];
  auto texture = surface.asFlutterMetalTexture;
  id<MTLTexture> metalTexture = (__bridge id)texture.texture;
  EXPECT_EQ(metalTexture.width, 100ul);
  EXPECT_EQ(metalTexture.height, 50ul);
  texture.destruction_callback(texture.user_data);
}

TEST(FlutterSurfaceManager, TestSurfaceLookupFromTexture) {
  TestView* testView = [[TestView alloc] init];
  FlutterSurfaceManager* surfaceManager = CreateSurfaceManager(testView);

  // Get back buffer, lookup should work for borrowed surfaces util present.
  auto surface = [surfaceManager surfaceForSize:CGSizeMake(100, 50)];

  // SurfaceManager should keep texture alive while borrowed.
  auto texture = surface.asFlutterMetalTexture;
  texture.destruction_callback(texture.user_data);

  FlutterMetalTexture dummyTexture{.texture_id = 1, .texture = nullptr, .user_data = nullptr};
  auto surface1 = [FlutterSurface fromFlutterMetalTexture:&dummyTexture];
  EXPECT_EQ(surface1, nil);

  auto surface2 = [FlutterSurface fromFlutterMetalTexture:&texture];
  EXPECT_EQ(surface2, surface);
}

TEST(FlutterSurfaceManager, BackBufferCacheDoesNotLeak) {
  TestView* testView = [[TestView alloc] init];
  FlutterSurfaceManager* surfaceManager = CreateSurfaceManager(testView);
  EXPECT_EQ(surfaceManager.backBufferCache.count, 0ul);

  auto surface1 = [surfaceManager surfaceForSize:CGSizeMake(100, 100)];
  [surfaceManager present:@[ CreatePresentInfo(surface1) ] notify:nil];

  EXPECT_EQ(surfaceManager.backBufferCache.count, 0ul);

  auto surface2 = [surfaceManager surfaceForSize:CGSizeMake(110, 110)];
  [surfaceManager present:@[ CreatePresentInfo(surface2) ] notify:nil];

  EXPECT_EQ(surfaceManager.backBufferCache.count, 1ul);

  auto surface3 = [surfaceManager surfaceForSize:CGSizeMake(120, 120)];
  [surfaceManager present:@[ CreatePresentInfo(surface3) ] notify:nil];

  // Cache should be cleaned during present and only contain the last visible
  // surface(s).
  EXPECT_EQ(surfaceManager.backBufferCache.count, 1ul);
  auto surfaceFromCache = [surfaceManager surfaceForSize:CGSizeMake(110, 110)];
  EXPECT_EQ(surfaceFromCache, surface2);

  [surfaceManager present:@[] notify:nil];
  EXPECT_EQ(surfaceManager.backBufferCache.count, 1ul);

  [surfaceManager present:@[] notify:nil];
  EXPECT_EQ(surfaceManager.backBufferCache.count, 0ul);
}

TEST(FlutterSurfaceManager, SurfacesAreRecycled) {
  TestView* testView = [[TestView alloc] init];
  FlutterSurfaceManager* surfaceManager = CreateSurfaceManager(testView);

  EXPECT_EQ(surfaceManager.frontSurfaces.count, 0ul);

  // Get first surface and present it.

  auto surface1 = [surfaceManager surfaceForSize:CGSizeMake(100, 100)];
  EXPECT_TRUE(CGSizeEqualToSize(surface1.size, CGSizeMake(100, 100)));

  EXPECT_EQ(surfaceManager.backBufferCache.count, 0ul);
  EXPECT_EQ(surfaceManager.frontSurfaces.count, 0ul);

  [surfaceManager present:@[ CreatePresentInfo(surface1) ] notify:nil];

  EXPECT_EQ(surfaceManager.backBufferCache.count, 0ul);
  EXPECT_EQ(surfaceManager.frontSurfaces.count, 1ul);
  EXPECT_EQ(testView.layer.sublayers.count, 1ul);

  // Get second surface and present it.

  auto surface2 = [surfaceManager surfaceForSize:CGSizeMake(100, 100)];
  EXPECT_TRUE(CGSizeEqualToSize(surface2.size, CGSizeMake(100, 100)));

  EXPECT_EQ(surfaceManager.backBufferCache.count, 0ul);

  [surfaceManager present:@[ CreatePresentInfo(surface2) ] notify:nil];

  // Check that current front surface returns to cache.
  EXPECT_EQ(surfaceManager.backBufferCache.count, 1ul);
  EXPECT_EQ(surfaceManager.frontSurfaces.count, 1ul);
  EXPECT_EQ(testView.layer.sublayers.count, 1ull);

  // Check that surface is properly reused.
  auto surface3 = [surfaceManager surfaceForSize:CGSizeMake(100, 100)];
  EXPECT_EQ(surface3, surface1);
}

TEST(FlutterSurfaceManager, LayerManagement) {
  TestView* testView = [[TestView alloc] init];
  FlutterSurfaceManager* surfaceManager = CreateSurfaceManager(testView);

  EXPECT_EQ(testView.layer.sublayers.count, 0ul);

  auto surface1_1 = [surfaceManager surfaceForSize:CGSizeMake(50, 30)];
  [surfaceManager present:@[ CreatePresentInfo(surface1_1, CGPointMake(20, 10)) ] notify:nil];

  EXPECT_EQ(testView.layer.sublayers.count, 1ul);
  EXPECT_TRUE(CGSizeEqualToSize(testView.presentedFrameSize, CGSizeMake(70, 40)));

  auto surface2_1 = [surfaceManager surfaceForSize:CGSizeMake(50, 30)];
  auto surface2_2 = [surfaceManager surfaceForSize:CGSizeMake(20, 20)];
  [surfaceManager present:@[
    CreatePresentInfo(surface2_1, CGPointMake(20, 10), 1),
    CreatePresentInfo(surface2_2, CGPointMake(40, 50), 2)
  ]
                   notify:nil];

  EXPECT_EQ(testView.layer.sublayers.count, 2ul);
  EXPECT_EQ([testView.layer.sublayers objectAtIndex:0].zPosition, 1.0);
  EXPECT_EQ([testView.layer.sublayers objectAtIndex:1].zPosition, 2.0);
  EXPECT_TRUE(CGSizeEqualToSize(testView.presentedFrameSize, CGSizeMake(70, 70)));

  auto surface3_1 = [surfaceManager surfaceForSize:CGSizeMake(50, 30)];
  [surfaceManager present:@[ CreatePresentInfo(surface3_1, CGPointMake(20, 10)) ] notify:nil];

  EXPECT_EQ(testView.layer.sublayers.count, 1ul);
  EXPECT_TRUE(CGSizeEqualToSize(testView.presentedFrameSize, CGSizeMake(70, 40)));

  // Check removal of all surfaces.
  [surfaceManager present:@[] notify:nil];
  EXPECT_EQ(testView.layer.sublayers.count, 0ul);
  EXPECT_TRUE(CGSizeEqualToSize(testView.presentedFrameSize, CGSizeMake(0, 0)));
}

}  // namespace flutter::testing
