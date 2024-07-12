// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHELL_PLATFORM_IOS_FRAMEWORK_SOURCE_FLUTTERRESTORATIONPLUGIN_H_
#define SHELL_PLATFORM_IOS_FRAMEWORK_SOURCE_FLUTTERRESTORATIONPLUGIN_H_

#import <UIKit/UIKit.h>

#import "flutter/shell/platform/darwin/common/framework/Headers/FlutterChannels.h"

@interface FlutterRestorationPlugin : NSObject
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
- (instancetype)initWithChannel:(FlutterMethodChannel*)channel
             restorationEnabled:(BOOL)waitForData NS_DESIGNATED_INITIALIZER;

@property(nonatomic, strong) NSData* restorationData;
- (void)markRestorationComplete;
- (void)reset;
@end
#endif  // SHELL_PLATFORM_IOS_FRAMEWORK_SOURCE_FLUTTERRESTORATIONPLUGIN_H_
