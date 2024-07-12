// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'dart:convert';
import 'dart:typed_data';

import 'dom.dart';
import 'util.dart';

const String ahemFontFamily = 'Ahem';
const String ahemFontUrl = '/assets/fonts/ahem.ttf';
const String robotoFontFamily = 'Roboto';
const String robotoTestFontUrl = '/assets/fonts/Roboto-Regular.ttf';

/// The list of test fonts, in the form of font family name - font file url pairs.
/// This list does not include embedded test fonts, which need to be loaded and
/// registered separately in [FontCollection.debugDownloadTestFonts].
const Map<String, String> testFontUrls = <String, String>{
  ahemFontFamily: ahemFontUrl,
  robotoFontFamily: robotoTestFontUrl,
  'RobotoVariable': '/assets/fonts/RobotoSlab-VariableFont_wght.ttf',
};

/// This class downloads assets over the network.
///
/// Assets are resolved relative to [assetsDir] inside the absolute base
/// specified by [assetBase] (optional).
///
/// By default, URLs are relative to the `<base>` of the current website.
class AssetManager {
  /// Initializes [AssetManager] with paths.
  AssetManager({
    this.assetsDir = _defaultAssetsDir,
    String? assetBase,
  })  : assert(
          assetBase == null || assetBase.endsWith('/'),
          '`assetBase` must end with a `/` character.',
        ),
        _assetBase = assetBase;

  static const String _defaultAssetsDir = 'assets';

  /// The directory containing the assets.
  final String assetsDir;

  /// The absolute base URL for assets.
  String? _assetBase;

  // Cache a value for `_assetBase` so we don't hit the DOM multiple times.
  String get _baseUrl => _assetBase ??= _deprecatedAssetBase ?? '';

  // Retrieves the `assetBase` value from the DOM.
  //
  // This warns the user and points them to the new initializeEngine style.
  String? get _deprecatedAssetBase {
    final DomHTMLMetaElement? meta = domWindow.document
        .querySelector('meta[name=assetBase]') as DomHTMLMetaElement?;

    final String? fallbackBaseUrl = meta?.content;

    if (fallbackBaseUrl != null) {
      // Warn users that they're using a deprecated configuration style...
      domWindow.console.warn('The `assetBase` meta tag is now deprecated.\n'
          'Use engineInitializer.initializeEngine(config) instead.\n'
          'See: https://docs.flutter.dev/development/platform-integration/web/initialization');
    }
    return fallbackBaseUrl;
  }

  /// Returns the URL to load the asset from, given the asset key.
  ///
  /// We URL-encode the asset URL in order to correctly issue the right
  /// HTTP request to the server.
  ///
  /// For example, if you have an asset in the file "assets/hello world.png",
  /// two things will happen. When the app is built, the asset will be copied
  /// to an asset directory with the file name URL-encoded. So our asset will
  /// be copied to something like "assets/hello%20world.png". To account for
  /// the assets being copied over with a URL-encoded name, the Flutter
  /// framework URL-encodes the asset key  so when it sends a request to the
  /// engine to load "assets/hello world.png", it actually sends a request to
  /// load "assets/hello%20world.png". However, on the web, if we try to load
  /// "assets/hello%20world.png", the request will be URL-decoded, we will
  /// request "assets/hello world.png", and the request will 404. Therefore, we
  /// must URL-encode the asset key *again* so when it is decoded, it is
  /// requesting the once-URL-encoded asset key.
  String getAssetUrl(String asset) {
    if (Uri.parse(asset).hasScheme) {
      return Uri.encodeFull(asset);
    }
    return Uri.encodeFull('$_baseUrl$assetsDir/$asset');
  }

  /// Loads an asset and returns the server response.
  Future<HttpFetchResponse> loadAsset(String asset) {
    return httpFetch(getAssetUrl(asset));
  }

  /// Loads an asset using an [DomXMLHttpRequest] and returns data as [ByteData].
  Future<ByteData> load(String asset) async {
    final String url = getAssetUrl(asset);
    final HttpFetchResponse response = await httpFetch(url);

    if (response.status == 404 && asset == 'AssetManifest.json') {
      printWarning('Asset manifest does not exist at `$url` - ignoring.');
      return Uint8List.fromList(utf8.encode('{}')).buffer.asByteData();
    }

    return (await response.payload.asByteBuffer()).asByteData();
  }
}

/// An asset manager that gives fake empty responses for assets.
class WebOnlyMockAssetManager extends AssetManager {
  /// Mock asset directory relative to base url.
  String defaultAssetsDir = '';

  /// Mock empty asset manifest.
  String defaultAssetManifest = '{}';

  /// Mock font manifest overridable for unit testing.
  String defaultFontManifest = '''
  [
   {
      "family":"$robotoFontFamily",
      "fonts":[{"asset":"$robotoTestFontUrl"}]
   },
   {
      "family":"$ahemFontFamily",
      "fonts":[{"asset":"$ahemFontUrl"}]
   }
  ]''';

  @override
  String get assetsDir => defaultAssetsDir;

  @override
  String getAssetUrl(String asset) => asset;

  @override
  Future<HttpFetchResponse> loadAsset(String asset) async {
    if (asset == getAssetUrl('AssetManifest.json')) {
      return MockHttpFetchResponse(
        url: asset,
        status: 200,
        payload: MockHttpFetchPayload(
          byteBuffer: _toByteData(utf8.encode(defaultAssetManifest)).buffer,
        ),
      );
    }
    if (asset == getAssetUrl('FontManifest.json')) {
      return MockHttpFetchResponse(
        url: asset,
        status: 200,
        payload: MockHttpFetchPayload(
          byteBuffer: _toByteData(utf8.encode(defaultFontManifest)).buffer,
        ),
      );
    }

    return MockHttpFetchResponse(
      url: asset,
      status: 404,
    );
  }

  @override
  Future<ByteData> load(String asset) {
    if (asset == getAssetUrl('AssetManifest.json')) {
      return Future<ByteData>.value(
          _toByteData(utf8.encode(defaultAssetManifest)));
    }
    if (asset == getAssetUrl('FontManifest.json')) {
      return Future<ByteData>.value(
          _toByteData(utf8.encode(defaultFontManifest)));
    }
    throw HttpFetchNoPayloadError(asset, status: 404);
  }

  ByteData _toByteData(List<int> bytes) {
    final ByteData byteData = ByteData(bytes.length);
    for (int i = 0; i < bytes.length; i++) {
      byteData.setUint8(i, bytes[i]);
    }
    return byteData;
  }
}
