// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:async';

import 'package:args/command_runner.dart';
import 'package:path/path.dart' as path;
import 'package:watcher/src/watch_event.dart';

import 'environment.dart';
import 'pipeline.dart';
import 'utils.dart';

enum RuntimeMode {
  profile,
  release,
}

const Map<String, String> targetAliases = <String, String>{
  'sdk': 'flutter/web_sdk',
  'web_sdk': 'flutter/web_sdk',
  'canvaskit': 'flutter/third_party/canvaskit:canvaskit_group',
  'canvaskit_chromium': 'flutter/third_party/canvaskit:canvaskit_chromium_group',
  'skwasm': 'flutter/lib/web_ui/skwasm',
  'archive': 'flutter/web_sdk:flutter_web_sdk_archive',
};

class BuildCommand extends Command<bool> with ArgUtils<bool> {
  BuildCommand() {
    argParser.addFlag(
      'watch',
      abbr: 'w',
      help: 'Run the build in watch mode so it rebuilds whenever a change is '
          'made. Disabled by default.',
    );
    argParser.addFlag(
      'host',
      help: 'Build the host build instead of the wasm build, which is '
          'currently needed for `flutter run --local-engine` to work.'
    );
    argParser.addFlag(
      'profile',
      help: 'Build in profile mode instead of release mode. In this mode, the '
          'output will be located at "out/wasm_profile".\nThis only applies to '
          'the wasm build. The host build is always built in release mode.',
    );
  }

  @override
  String get name => 'build';

  @override
  String get description => 'Build the Flutter web engine.';

  bool get isWatchMode => boolArg('watch');

  bool get host => boolArg('host');

  RuntimeMode get runtimeMode =>
      boolArg('profile') ? RuntimeMode.profile : RuntimeMode.release;

  List<String> get targets => argResults?.rest ?? <String>[];

  @override
  FutureOr<bool> run() async {
    final FilePath libPath = FilePath.fromWebUi('lib');
    final List<PipelineStep> steps = <PipelineStep>[
      GnPipelineStep(
        host: host,
        runtimeMode: runtimeMode,
      ),
      NinjaPipelineStep(
        host: host,
        runtimeMode: runtimeMode,
        targets: targets.map((String target) => targetAliases[target] ?? target),
      ),
    ];
    final Pipeline buildPipeline = Pipeline(steps: steps);
    await buildPipeline.run();

    if (isWatchMode) {
      print('Initial build done!');
      print('Watching directory: ${libPath.relativeToCwd}/');
      await PipelineWatcher(
        dir: libPath.absolute,
        pipeline: buildPipeline,
        // Ignore font files that are copied whenever tests run.
        ignore: (WatchEvent event) => event.path.endsWith('.ttf'),
      ).start();
    }
    return true;
  }
}

/// Runs `gn`.
///
/// Not safe to interrupt as it may leave the `out/` directory in a corrupted
/// state. GN is pretty quick though, so it's OK to not support interruption.
class GnPipelineStep extends ProcessStep {
  GnPipelineStep({
    required this.host,
    required this.runtimeMode,
  });

  final bool host;
  final RuntimeMode runtimeMode;

  @override
  String get description => 'gn';

  @override
  bool get isSafeToInterrupt => false;

  String get runtimeModeFlag {
    switch (runtimeMode) {
      case RuntimeMode.profile:
        return 'profile';
      case RuntimeMode.release:
        return 'release';
    }
  }

  List<String> get _gnArgs {
    if (host) {
      return <String>[
        '--unoptimized',
        '--full-dart-sdk',
      ];
    } else {
      return <String>[
        '--web',
        '--runtime-mode=$runtimeModeFlag',
      ];
    }
  }

  @override
  Future<ProcessManager> createProcess() {
    print('Running gn...');
    return startProcess(
      path.join(environment.flutterDirectory.path, 'tools', 'gn'),
      _gnArgs,
    );
  }
}

/// Runs `autoninja`.
///
/// Can be safely interrupted.
class NinjaPipelineStep extends ProcessStep {
  NinjaPipelineStep({
    required this.host,
    required this.runtimeMode,
    required this.targets,
  });

  @override
  String get description => 'ninja';

  @override
  bool get isSafeToInterrupt => true;

  final bool host;
  final Iterable<String> targets;
  final RuntimeMode runtimeMode;

  String get buildDirectory {
    if (host) {
      return environment.hostDebugUnoptDir.path;
    }
    switch (runtimeMode) {
      case RuntimeMode.profile:
        return environment.wasmProfileOutDir.path;
      case RuntimeMode.release:
        return environment.wasmReleaseOutDir.path;
    }
  }

  @override
  Future<ProcessManager> createProcess() {
    print('Running autoninja...');
    return startProcess(
      'autoninja',
      <String>[
        '-C',
        buildDirectory,
        ...targets,
      ],
    );
  }
}
