// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "flutter/shell/platform/darwin/common/framework/Headers/FlutterMacros.h"
#import "flutter/shell/platform/darwin/common/framework/Headers/FlutterTexture.h"

#if FLUTTER_RUNTIME_MODE == FLUTTER_RUNTIME_MODE_DEBUG
FLUTTER_DARWIN_EXPORT
#endif

/**
 * Wrapper around a weakly held collection of registered textures.
 *
 * Avoids a retain cycle between plugins and the engine.
 */
@interface FlutterTextureRegistryRelay : NSObject <FlutterTextureRegistry>

/**
 * A weak reference to a FlutterEngine that will be passed texture registration.
 */
@property(nonatomic, assign) NSObject<FlutterTextureRegistry>* parent;
- (instancetype)initWithParent:(NSObject<FlutterTextureRegistry>*)parent;
@end
