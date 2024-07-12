// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:typed_data';

import 'package:ui/ui.dart' as ui;

import '../profiler.dart';
import '../util.dart';
import 'canvas.dart';
import 'canvaskit_api.dart';
import 'image.dart';
import 'skia_object_cache.dart';
import 'surface.dart';
import 'surface_factory.dart';

/// Implements [ui.Picture] on top of [SkPicture].
///
/// Unlike most other [ManagedSkiaObject] implementations, instances of this
/// class may have their Skia counterparts deleted before finalization registry
/// or [SkiaObjectCache] decide to delete it.
class CkPicture extends ManagedSkiaObject<SkPicture> implements ui.Picture {
  CkPicture(SkPicture super.picture, this.cullRect, this._snapshot) :
    assert(
      browserSupportsFinalizationRegistry && _snapshot == null ||
          _snapshot != null,
      'If the browser does not support FinalizationRegistry (WeakRef), then we must have a picture snapshot to be able to resurrect it.',
    );

  final ui.Rect? cullRect;
  final CkPictureSnapshot? _snapshot;

  @override
  int get approximateBytesUsed => 0;

  @override
  bool get debugDisposed {
    if (assertionsEnabled) {
      return _isDisposed;
    }
    throw StateError('Picture.debugDisposed is only available when asserts are enabled.');
  }

  /// This is set to true when [dispose] is called and is never reset back to
  /// false.
  ///
  /// This extra flag is necessary on top of [rawSkiaObject] because
  /// [rawSkiaObject] being null does not indicate permanent deletion.
  bool _isDisposed = false;

  /// The stack trace taken when [dispose] was called.
  ///
  /// Returns null if [dispose] has not been called. Returns null in non-debug
  /// modes.
  StackTrace? _debugDisposalStackTrace;

  /// Throws an [AssertionError] if this picture was disposed.
  ///
  /// The [mainErrorMessage] is used as the first line in the error message. It
  /// is expected to end with a period, e.g. "Failed to draw picture." The full
  /// message will also explain that the error is due to the fact that the
  /// picture was disposed and include the stack trace taken when the picture
  /// was disposed.
  bool debugCheckNotDisposed(String mainErrorMessage) {
    if (_isDisposed) {
      throw StateError(
        '$mainErrorMessage\n'
        'The picture has been disposed. When the picture was disposed the '
        'stack trace was:\n'
        '$_debugDisposalStackTrace',
      );
    }
    return true;
  }

  @override
  void dispose() {
    assert(debugCheckNotDisposed('Cannot dispose picture.'));
    assert(() {
      _debugDisposalStackTrace = StackTrace.current;
      return true;
    }());
    ui.Picture.onDispose?.call(this);
    if (Instrumentation.enabled) {
      Instrumentation.instance.incrementCounter('Picture disposed');
    }
    _isDisposed = true;
    _snapshot?.dispose();

    // Emulate what SkiaObjectCache does.
    rawSkiaObject?.delete();
    rawSkiaObject = null;
  }

  @override
  Future<ui.Image> toImage(int width, int height) async {
    return toImageSync(width, height);
  }

  @override
  CkImage toImageSync(int width, int height) {
    assert(debugCheckNotDisposed('Cannot convert picture to image.'));

    final Surface surface = SurfaceFactory.instance.pictureToImageSurface;
    final CkSurface ckSurface =
      surface.createOrUpdateSurface(ui.Size(width.toDouble(), height.toDouble()));
    final CkCanvas ckCanvas = ckSurface.getCanvas();
    ckCanvas.clear(const ui.Color(0x00000000));
    ckCanvas.drawPicture(this);
    final SkImage skImage = ckSurface.surface.makeImageSnapshot();
    final SkImageInfo imageInfo = SkImageInfo(
      alphaType: canvasKit.AlphaType.Premul,
      colorType: canvasKit.ColorType.RGBA_8888,
      colorSpace: SkColorSpaceSRGB,
      width: width.toDouble(),
      height: height.toDouble(),
    );
    final Uint8List pixels = skImage.readPixels(0, 0, imageInfo);
    final SkImage? rasterImage = canvasKit.MakeImage(imageInfo, pixels, (4 * width).toDouble());
    if (rasterImage == null) {
      throw StateError('Unable to convert image pixels into SkImage.');
    }
    return CkImage(rasterImage);
  }

  @override
  bool get isResurrectionExpensive => true;

  @override
  SkPicture createDefault() {
    // The default object is supplied in the constructor.
    throw StateError('Unreachable code');
  }

  @override
  SkPicture resurrect() {
    // If a picture has been explicitly disposed of, it can no longer be
    // resurrected. An attempt to resurrect after the framework told the
    // engine to dispose of the picture likely indicates a bug in the engine.
    assert(debugCheckNotDisposed('Cannot resurrect picture.'));
    return _snapshot!.toPicture();
  }

  @override
  void delete() {
    // This method may be called after [dispose], in which case there's nothing
    // left to do. The Skia object is deleted permanently.
    if (!_isDisposed) {
      rawSkiaObject?.delete();
    }
  }
}
