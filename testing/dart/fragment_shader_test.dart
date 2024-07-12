// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';
import 'dart:collection';
import 'dart:convert' as convert;
import 'dart:io';
import 'dart:typed_data';
import 'dart:ui';

import 'package:litetest/litetest.dart';
import 'package:path/path.dart' as path;

import 'shader_test_file_utils.dart';

void main() async {
  bool assertsEnabled = false;
  assert(() {
    assertsEnabled = true;
    return true;
  }());

  test('impellerc produces reasonable JSON encoded IPLR files', () async {
    final Directory directory = shaderDirectory('iplr-json');
    final Object? rawData = convert.json.decode(
      File(path.join(directory.path, 'ink_sparkle.frag.iplr')).readAsStringSync());

    expect(rawData is Map<String, Object?>, true);

    final Map<String, Object?> data = rawData! as Map<String, Object?>;
    expect(data['sksl'] is String, true);
    expect(data['uniforms'] is List<Object?>, true);

    final Object? rawUniformData = (data['uniforms']! as List<Object?>)[0];

    expect(rawUniformData is Map<String, Object?>, true);

    final Map<String, Object?> uniformData = rawUniformData! as Map<String, Object?>;

    expect(uniformData['location'] is int, true);
  });

  test('FragmentShader setSampler throws with out-of-bounds index', () async {
    final FragmentProgram program = await FragmentProgram.fromAsset(
      'blue_green_sampler.frag.iplr',
    );
    final Image blueGreenImage = await _createBlueGreenImage();
    final FragmentShader fragmentShader = program.fragmentShader();

    try {
      fragmentShader.setImageSampler(1, blueGreenImage);
      fail('Unreachable');
    } catch (e) {
      expect(e, contains('Sampler index out of bounds'));
    } finally {
      fragmentShader.dispose();
      blueGreenImage.dispose();
    }
  });

  test('FragmentShader with sampler asserts if sampler is missing when assigned to paint', () async {
    if (!assertsEnabled) {
      return;
    }
    final FragmentProgram program = await FragmentProgram.fromAsset(
      'blue_green_sampler.frag.iplr',
    );
    final FragmentShader fragmentShader = program.fragmentShader();

    try {
      Paint().shader = fragmentShader;
      fail('Expected to throw');
    } catch (err) {
      expect(err.toString(), contains('Invalid FragmentShader blue_green_sampler.frag.iplr'));
    } finally {
      fragmentShader.dispose();
    }
  });

  test('Disposed FragmentShader on Paint', () async {
    final FragmentProgram program = await FragmentProgram.fromAsset(
      'blue_green_sampler.frag.iplr',
    );
    final Image blueGreenImage = await _createBlueGreenImage();

    final FragmentShader shader = program.fragmentShader()
      ..setImageSampler(0, blueGreenImage);
    shader.dispose();
    try {
      final Paint paint = Paint()..shader = shader;  // ignore: unused_local_variable
      if (assertsEnabled) {
        fail('Unreachable');
      }
    } catch (e) {
      expect(e.toString(), contains('Attempted to set a disposed shader'));
    }
    blueGreenImage.dispose();
  });

  test('Disposed FragmentShader setFloat', () async {
    final FragmentProgram program = await FragmentProgram.fromAsset(
      'uniforms.frag.iplr',
    );
    final FragmentShader shader = program.fragmentShader()
      ..setFloat(0, 0.0);
    shader.dispose();
    try {
      shader.setFloat(0, 0.0);
      if (assertsEnabled) {
        fail('Unreachable');
      }
    } catch (e) {
      if (assertsEnabled) {
        expect(
          e.toString(),
          contains('Tried to accesss uniforms on a disposed Shader'),
        );
      } else {
        expect(e is RangeError, true);
      }
    }
  });

  test('Disposed FragmentShader setImageSampler', () async {
    final FragmentProgram program = await FragmentProgram.fromAsset(
      'blue_green_sampler.frag.iplr',
    );
    final Image blueGreenImage = await _createBlueGreenImage();

    final FragmentShader shader = program.fragmentShader()
      ..setImageSampler(0, blueGreenImage);
    shader.dispose();
    try {
      shader.setImageSampler(0, blueGreenImage);
      if (assertsEnabled) {
        fail('Unreachable');
      }
    } on AssertionError catch (e) {
      expect(
        e.toString(),
        contains('Tried to access uniforms on a disposed Shader'),
      );
    } on StateError catch (e) {
      expect(
        e.toString(),
        contains('the native peer has been collected'),
      );
    }
    blueGreenImage.dispose();
  });

  test('Disposed FragmentShader dispose', () async {
    final FragmentProgram program = await FragmentProgram.fromAsset(
      'uniforms.frag.iplr',
    );
    final FragmentShader shader = program.fragmentShader()
      ..setFloat(0, 0.0);
    shader.dispose();
    try {
      shader.dispose();
      if (assertsEnabled) {
        fail('Unreachable');
      }
    } catch (e) {
      if (assertsEnabled) {
        expect(e is AssertionError, true);
      } else {
        expect(e is StateError, true);
      }
    }
  });

  test('FragmentShader simple shader renders correctly', () async {
    final FragmentProgram program = await FragmentProgram.fromAsset(
      'functions.frag.iplr',
    );
    final FragmentShader shader = program.fragmentShader()
      ..setFloat(0, 1.0);
    await _expectShaderRendersGreen(shader);
    shader.dispose();
  });

  test('Reused FragmentShader simple shader renders correctly', () async {
    final FragmentProgram program = await FragmentProgram.fromAsset(
      'functions.frag.iplr',
    );
    final FragmentShader shader = program.fragmentShader()
      ..setFloat(0, 1.0);
    await _expectShaderRendersGreen(shader);

    shader.setFloat(0, 0.0);
    await _expectShaderRendersBlack(shader);

    shader.dispose();
  });

  test('FragmentShader blue-green image renders green', () async {
    final FragmentProgram program = await FragmentProgram.fromAsset(
      'blue_green_sampler.frag.iplr',
    );
    final Image blueGreenImage = await _createBlueGreenImage();
    final FragmentShader shader = program.fragmentShader()
      ..setImageSampler(0, blueGreenImage);
    await _expectShaderRendersGreen(shader);
    shader.dispose();
    blueGreenImage.dispose();
  });

  test('FragmentShader blue-green image renders green - GPU image', () async {
    final FragmentProgram program = await FragmentProgram.fromAsset(
      'blue_green_sampler.frag.iplr',
    );
    final Image blueGreenImage = _createBlueGreenImageSync();
    final FragmentShader shader = program.fragmentShader()
      ..setImageSampler(0, blueGreenImage);
    await _expectShaderRendersGreen(shader);
    shader.dispose();
    blueGreenImage.dispose();
  });

  test('FragmentShader with uniforms renders correctly', () async {
    final FragmentProgram program = await FragmentProgram.fromAsset(
      'uniforms.frag.iplr',
    );

    final FragmentShader shader = program.fragmentShader()
      ..setFloat(0, 0.0)
      ..setFloat(1, 0.25)
      ..setFloat(2, 0.75)
      ..setFloat(3, 0.0)
      ..setFloat(4, 0.0)
      ..setFloat(5, 0.0)
      ..setFloat(6, 1.0);

    final ByteData renderedBytes = (await _imageByteDataFromShader(
      shader: shader,
    ))!;

    expect(toFloat(renderedBytes.getUint8(0)), closeTo(0.0, epsilon));
    expect(toFloat(renderedBytes.getUint8(1)), closeTo(0.25, epsilon));
    expect(toFloat(renderedBytes.getUint8(2)), closeTo(0.75, epsilon));
    expect(toFloat(renderedBytes.getUint8(3)), closeTo(1.0, epsilon));

    shader.dispose();
  });

  test('FragmentShader shader with array uniforms renders correctly', () async {
    final FragmentProgram program = await FragmentProgram.fromAsset(
      'uniform_arrays.frag.iplr',
    );

    final FragmentShader shader = program.fragmentShader();
    for (int i = 0; i < 20; i++) {
      shader.setFloat(i, i.toDouble());
    }

    await _expectShaderRendersGreen(shader);
    shader.dispose();
  });

  test('FragmentShader The ink_sparkle shader is accepted', () async {
    final FragmentProgram program = await FragmentProgram.fromAsset(
      'ink_sparkle.frag.iplr',
    );
    final FragmentShader shader = program.fragmentShader();

    await _imageByteDataFromShader(shader: shader);

    // Testing that no exceptions are thrown. Tests that the ink_sparkle shader
    // produces the correct pixels are in the framework.
    shader.dispose();
  });

  test('FragmentShader Uniforms are sorted correctly', () async {
    final FragmentProgram program = await FragmentProgram.fromAsset(
      'uniforms_sorted.frag.iplr',
    );

    // The shader will not render green if the compiler doesn't keep the
    // uniforms in the right order.
    final FragmentShader shader = program.fragmentShader();
    for (int i = 0; i < 32; i++) {
      shader.setFloat(i, i.toDouble());
    }

    await _expectShaderRendersGreen(shader);

    shader.dispose();
  });

  test('fromAsset throws an exception on invalid assetKey', () async {
    bool throws = false;
    try {
      await FragmentProgram.fromAsset(
        '<invalid>',
      );
    } catch (e) {
      throws = true;
    }
    expect(throws, equals(true));
  });

  test('fromAsset throws an exception on invalid data', () async {
    bool throws = false;
    try {
      await FragmentProgram.fromAsset(
        'DashInNooglerHat.jpg',
      );
    } catch (e) {
      throws = true;
    }
    expect(throws, equals(true));
  });

  test('FragmentShader user defined functions do not redefine builtins', () async {
    final FragmentProgram program = await FragmentProgram.fromAsset(
      'no_builtin_redefinition.frag.iplr',
    );
    final FragmentShader shader = program.fragmentShader()
      ..setFloat(0, 1.0);
    await _expectShaderRendersGreen(shader);
    shader.dispose();
  });

  test('FragmentShader fromAsset accepts a shader with no uniforms', () async {
    final FragmentProgram program = await FragmentProgram.fromAsset(
      'no_uniforms.frag.iplr',
    );
    final FragmentShader shader = program.fragmentShader();
    await _expectShaderRendersGreen(shader);
    shader.dispose();
  });

  // Test all supported GLSL ops. See lib/spirv/lib/src/constants.dart
  final Map<String, FragmentProgram> iplrSupportedGLSLOpShaders = await _loadShaderAssets(
    path.join('supported_glsl_op_shaders', 'iplr'),
    '.iplr',
  );
  expect(iplrSupportedGLSLOpShaders.isNotEmpty, true);
  _expectFragmentShadersRenderGreen(iplrSupportedGLSLOpShaders);

  // Test all supported instructions. See lib/spirv/lib/src/constants.dart
  final Map<String, FragmentProgram> iplrSupportedOpShaders = await _loadShaderAssets(
    path.join('supported_op_shaders', 'iplr'),
    '.iplr',
  );
  expect(iplrSupportedOpShaders.isNotEmpty, true);
  _expectFragmentShadersRenderGreen(iplrSupportedOpShaders);
}

// Expect that all of the shaders in this folder render green.
// Keeping the outer loop of the test synchronous allows for easy printing
// of the file name within the test case.
void _expectFragmentShadersRenderGreen(Map<String, FragmentProgram> programs) {
  for (final String key in programs.keys) {
    test('FragmentProgram $key renders green', () async {
      final FragmentProgram program = programs[key]!;
      final FragmentShader shader = program.fragmentShader()
        ..setFloat(0, 1.0);
      await _expectShaderRendersGreen(shader);
      shader.dispose();
    });
  }
}

Future<void> _expectShaderRendersColor(Shader shader, Color color) async {
  final ByteData renderedBytes = (await _imageByteDataFromShader(
    shader: shader,
    imageDimension: _shaderImageDimension,
  ))!;
  for (final int c in renderedBytes.buffer.asUint32List()) {
    expect(toHexString(c), toHexString(color.value));
  }
}

// Expects that a shader only outputs the color green.
Future<void> _expectShaderRendersGreen(Shader shader) {
  return _expectShaderRendersColor(shader, _greenColor);
}

Future<void> _expectShaderRendersBlack(Shader shader) {
  return _expectShaderRendersColor(shader, _blackColor);
}

Future<ByteData?> _imageByteDataFromShader({
  required Shader shader,
  int imageDimension = 100,
}) async {
  final PictureRecorder recorder = PictureRecorder();
  final Canvas canvas = Canvas(recorder);
  final Paint paint = Paint()..shader = shader;
  canvas.drawPaint(paint);
  final Picture picture = recorder.endRecording();
  final Image image = await picture.toImage(
    imageDimension,
    imageDimension,
  );
  return image.toByteData();
}

// Loads the path and spirv content of the files at
// $FLUTTER_BUILD_DIRECTORY/gen/flutter/lib/spirv/test/$leafFolderName
// This is synchronous so that tests can be inside of a loop with
// the proper test name.
Future<Map<String, FragmentProgram>> _loadShaderAssets(
    String leafFolderName,
    String ext,
  ) async {
  final Map<String, FragmentProgram> out = SplayTreeMap<String, FragmentProgram>();

  final Directory directory = shaderDirectory(leafFolderName);
  if (!directory.existsSync()) {
    return out;
  }

  await Future.forEach(
    directory
      .listSync()
      .where((FileSystemEntity entry) => path.extension(entry.path) == ext),
    (FileSystemEntity entry) async {
      final String key = path.basenameWithoutExtension(entry.path);
      out[key] = await FragmentProgram.fromAsset(
        path.basename(entry.path),
      );
    },
  );
  return out;
}

// Arbitrary, but needs to be greater than 1 for frag coord tests.
const int _shaderImageDimension = 4;

const Color _greenColor = Color(0xFF00FF00);
const Color _blackColor = Color(0xFF000000);

// Precision for checking uniform values.
const double epsilon = 0.5 / 255.0;

// Maps an int value from 0-255 to a double value of 0.0 to 1.0.
double toFloat(int v) => v.toDouble() / 255.0;

String toHexString(int color) => '#${color.toRadixString(16)}';

// 10x10 image where the left half is blue and the right half is
// green.
Future<Image> _createBlueGreenImage() async {
  const int length = 10;
  const int bytesPerPixel = 4;
  final Uint8List pixels = Uint8List(length * length * bytesPerPixel);
  int i = 0;
  for (int y = 0; y < length; y++) {
    for (int x = 0; x < length; x++) {
      if (x < length/2) {
        pixels[i+2] = 0xFF;  // blue channel
      } else {
        pixels[i+1] = 0xFF;  // green channel
      }
      pixels[i+3] = 0xFF;  // alpha channel
      i += bytesPerPixel;
    }
  }
  final ImageDescriptor descriptor = ImageDescriptor.raw(
    await ImmutableBuffer.fromUint8List(pixels),
    width: length,
    height: length,
    pixelFormat: PixelFormat.rgba8888,
  );
  final Codec codec = await descriptor.instantiateCodec();
  final FrameInfo frame = await codec.getNextFrame();
  return frame.image;
}

// A 10x10 image where the left half is blue and the right half is green.
Image _createBlueGreenImageSync() {
  final PictureRecorder recorder = PictureRecorder();
  final Canvas canvas = Canvas(recorder);
  canvas.drawRect(const Rect.fromLTWH(0, 0, 5, 10), Paint()..color = const Color(0xFF0000FF));
  canvas.drawRect(const Rect.fromLTWH(5, 0, 5, 10), Paint()..color = const Color(0xFF00FF00));
  final Picture picture = recorder.endRecording();
  try {
    return picture.toImageSync(10, 10);
  } finally {
    picture.dispose();
  }
}

// Ignore invalid utf8 since file is not actually text.
String readAsStringLossy(File file) {
  return convert.utf8.decode(file.readAsBytesSync(), allowMalformed: true);
}
