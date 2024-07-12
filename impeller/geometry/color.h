// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <stdint.h>
#include <array>
#include <cstdlib>
#include <ostream>
#include <type_traits>
#include "impeller/geometry/scalar.h"

#define IMPELLER_FOR_EACH_BLEND_MODE(V) \
  V(Clear)                              \
  V(Source)                             \
  V(Destination)                        \
  V(SourceOver)                         \
  V(DestinationOver)                    \
  V(SourceIn)                           \
  V(DestinationIn)                      \
  V(SourceOut)                          \
  V(DestinationOut)                     \
  V(SourceATop)                         \
  V(DestinationATop)                    \
  V(Xor)                                \
  V(Plus)                               \
  V(Modulate)                           \
  V(Screen)                             \
  V(Overlay)                            \
  V(Darken)                             \
  V(Lighten)                            \
  V(ColorDodge)                         \
  V(ColorBurn)                          \
  V(HardLight)                          \
  V(SoftLight)                          \
  V(Difference)                         \
  V(Exclusion)                          \
  V(Multiply)                           \
  V(Hue)                                \
  V(Saturation)                         \
  V(Color)                              \
  V(Luminosity)

namespace impeller {

struct ColorHSB;
struct Vector4;

enum class YUVColorSpace { kBT601LimitedRange, kBT601FullRange };

/// All blend modes assume that both the source (fragment output) and
/// destination (first color attachment) have colors with premultiplied alpha.
enum class BlendMode {
  // The following blend modes are able to be used as pipeline blend modes or
  // via `BlendFilterContents`.
  kClear,
  kSource,
  kDestination,
  kSourceOver,
  kDestinationOver,
  kSourceIn,
  kDestinationIn,
  kSourceOut,
  kDestinationOut,
  kSourceATop,
  kDestinationATop,
  kXor,
  kPlus,
  kModulate,

  // The following blend modes use equations that are not available for
  // pipelines on most graphics devices without extensions, and so they are
  // only able to be used via `BlendFilterContents`.
  kScreen,
  kOverlay,
  kDarken,
  kLighten,
  kColorDodge,
  kColorBurn,
  kHardLight,
  kSoftLight,
  kDifference,
  kExclusion,
  kMultiply,
  kHue,
  kSaturation,
  kColor,
  kLuminosity,

  kLast = kLuminosity,
};

/**
 *  Represents a RGBA color
 */
struct Color {
  /**
   *  The red color component (0 to 1)
   */
  Scalar red = 0.0;

  /**
   *  The green color component (0 to 1)
   */
  Scalar green = 0.0;

  /**
   *  The blue color component (0 to 1)
   */
  Scalar blue = 0.0;

  /**
   *  The alpha component of the color (0 to 1)
   */
  Scalar alpha = 0.0;

  constexpr Color() {}

  explicit Color(const ColorHSB& hsbColor);

  Color(const Vector4& value);

  constexpr Color(Scalar r, Scalar g, Scalar b, Scalar a)
      : red(r), green(g), blue(b), alpha(a) {}

  static constexpr Color MakeRGBA8(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return Color(
        static_cast<Scalar>(r) / 255.0f, static_cast<Scalar>(g) / 255.0f,
        static_cast<Scalar>(b) / 255.0f, static_cast<Scalar>(a) / 255.0f);
  }

  /// @brief Convert this color to a 32-bit representation.
  static constexpr uint32_t ToIColor(Color color) {
    return (((std::lround(color.alpha * 255.0f) & 0xff) << 24) |
            ((std::lround(color.red * 255.0f) & 0xff) << 16) |
            ((std::lround(color.green * 255.0f) & 0xff) << 8) |
            ((std::lround(color.blue * 255.0f) & 0xff) << 0)) &
           0xFFFFFFFF;
  }

  constexpr bool operator==(const Color& c) const {
    return ScalarNearlyEqual(red, c.red) && ScalarNearlyEqual(green, c.green) &&
           ScalarNearlyEqual(blue, c.blue) && ScalarNearlyEqual(alpha, c.alpha);
  }

  constexpr Color Premultiply() const {
    return {red * alpha, green * alpha, blue * alpha, alpha};
  }

  constexpr Color Unpremultiply() const {
    if (ScalarNearlyEqual(alpha, 0.0f)) {
      return Color::BlackTransparent();
    }
    return {red / alpha, green / alpha, blue / alpha, alpha};
  }

  /**
   * @brief Return a color that is linearly interpolated between colors a
   * and b, according to the value of t.
   *
   * @param a The lower color.
   * @param b The upper color.
   * @param t A value between 0.0f and 1.0f, inclusive.
   * @return constexpr Color
   */
  constexpr static Color lerp(Color a, Color b, Scalar t) {
    Scalar tt = 1.0f - t;
    return {a.red * tt + b.red * t, a.green * tt + b.green * t,
            a.blue * tt + b.blue * t, a.alpha * tt + b.alpha * t};
  }

  /**
   * @brief Convert to R8G8B8A8 representation.
   *
   * @return constexpr std::array<u_int8, 4>
   */
  constexpr std::array<uint8_t, 4> ToR8G8B8A8() const {
    uint8_t r = std::round(red * 255.0f);
    uint8_t g = std::round(green * 255.0f);
    uint8_t b = std::round(blue * 255.0f);
    uint8_t a = std::round(alpha * 255.0f);
    return {r, g, b, a};
  }

  static constexpr Color White() { return {1.0f, 1.0f, 1.0f, 1.0f}; }

  static constexpr Color Black() { return {0.0f, 0.0f, 0.0f, 1.0f}; }

  static constexpr Color WhiteTransparent() { return {1.0f, 1.0f, 1.0f, 0.0f}; }

  static constexpr Color BlackTransparent() { return {0.0f, 0.0f, 0.0f, 0.0f}; }

  static constexpr Color Red() { return {1.0f, 0.0f, 0.0f, 1.0f}; }

  static constexpr Color Green() { return {0.0f, 1.0f, 0.0f, 1.0f}; }

  static constexpr Color Blue() { return {0.0f, 0.0f, 1.0f, 1.0f}; }

  constexpr Color WithAlpha(Scalar new_alpha) const {
    return {red, green, blue, new_alpha};
  }

  static constexpr Color AliceBlue() {
    return {240.0f / 255.0f, 248.0f / 255.0f, 255.0f / 255.0f, 1.0f};
  }

  static constexpr Color AntiqueWhite() {
    return {250.0f / 255.0f, 235.0f / 255.0f, 215.0f / 255.0f, 1.0f};
  }

  static constexpr Color Aqua() {
    return {0.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f, 1.0f};
  }

  static constexpr Color AquaMarine() {
    return {127.0f / 255.0f, 255.0f / 255.0f, 212.0f / 255.0f, 1.0f};
  }

  static constexpr Color Azure() {
    return {240.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f, 1.0f};
  }

  static constexpr Color Beige() {
    return {245.0f / 255.0f, 245.0f / 255.0f, 220.0f / 255.0f, 1.0f};
  }

  static constexpr Color Bisque() {
    return {255.0f / 255.0f, 228.0f / 255.0f, 196.0f / 255.0f, 1.0f};
  }

  static constexpr Color BlanchedAlmond() {
    return {255.0f / 255.0f, 235.0f / 255.0f, 205.0f / 255.0f, 1.0f};
  }

  static constexpr Color BlueViolet() {
    return {138.0f / 255.0f, 43.0f / 255.0f, 226.0f / 255.0f, 1.0f};
  }

  static constexpr Color Brown() {
    return {165.0f / 255.0f, 42.0f / 255.0f, 42.0f / 255.0f, 1.0f};
  }

  static constexpr Color BurlyWood() {
    return {222.0f / 255.0f, 184.0f / 255.0f, 135.0f / 255.0f, 1.0f};
  }

  static constexpr Color CadetBlue() {
    return {95.0f / 255.0f, 158.0f / 255.0f, 160.0f / 255.0f, 1.0f};
  }

  static constexpr Color Chartreuse() {
    return {127.0f / 255.0f, 255.0f / 255.0f, 0.0f / 255.0f, 1.0f};
  }

  static constexpr Color Chocolate() {
    return {210.0f / 255.0f, 105.0f / 255.0f, 30.0f / 255.0f, 1.0f};
  }

  static constexpr Color Coral() {
    return {255.0f / 255.0f, 127.0f / 255.0f, 80.0f / 255.0f, 1.0f};
  }

  static constexpr Color CornflowerBlue() {
    return {100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f, 1.0f};
  }

  static constexpr Color Cornsilk() {
    return {255.0f / 255.0f, 248.0f / 255.0f, 220.0f / 255.0f, 1.0f};
  }

  static constexpr Color Crimson() {
    return {220.0f / 255.0f, 20.0f / 255.0f, 60.0f / 255.0f, 1.0f};
  }

  static constexpr Color Cyan() {
    return {0.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkBlue() {
    return {0.0f / 255.0f, 0.0f / 255.0f, 139.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkCyan() {
    return {0.0f / 255.0f, 139.0f / 255.0f, 139.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkGoldenrod() {
    return {184.0f / 255.0f, 134.0f / 255.0f, 11.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkGray() {
    return {169.0f / 255.0f, 169.0f / 255.0f, 169.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkGreen() {
    return {0.0f / 255.0f, 100.0f / 255.0f, 0.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkGrey() {
    return {169.0f / 255.0f, 169.0f / 255.0f, 169.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkKhaki() {
    return {189.0f / 255.0f, 183.0f / 255.0f, 107.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkMagenta() {
    return {139.0f / 255.0f, 0.0f / 255.0f, 139.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkOliveGreen() {
    return {85.0f / 255.0f, 107.0f / 255.0f, 47.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkOrange() {
    return {255.0f / 255.0f, 140.0f / 255.0f, 0.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkOrchid() {
    return {153.0f / 255.0f, 50.0f / 255.0f, 204.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkRed() {
    return {139.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkSalmon() {
    return {233.0f / 255.0f, 150.0f / 255.0f, 122.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkSeagreen() {
    return {143.0f / 255.0f, 188.0f / 255.0f, 143.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkSlateBlue() {
    return {72.0f / 255.0f, 61.0f / 255.0f, 139.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkSlateGray() {
    return {47.0f / 255.0f, 79.0f / 255.0f, 79.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkSlateGrey() {
    return {47.0f / 255.0f, 79.0f / 255.0f, 79.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkTurquoise() {
    return {0.0f / 255.0f, 206.0f / 255.0f, 209.0f / 255.0f, 1.0f};
  }

  static constexpr Color DarkViolet() {
    return {148.0f / 255.0f, 0.0f / 255.0f, 211.0f / 255.0f, 1.0f};
  }

  static constexpr Color DeepPink() {
    return {255.0f / 255.0f, 20.0f / 255.0f, 147.0f / 255.0f, 1.0f};
  }

  static constexpr Color DeepSkyBlue() {
    return {0.0f / 255.0f, 191.0f / 255.0f, 255.0f / 255.0f, 1.0f};
  }

  static constexpr Color DimGray() {
    return {105.0f / 255.0f, 105.0f / 255.0f, 105.0f / 255.0f, 1.0f};
  }

  static constexpr Color DimGrey() {
    return {105.0f / 255.0f, 105.0f / 255.0f, 105.0f / 255.0f, 1.0f};
  }

  static constexpr Color DodgerBlue() {
    return {30.0f / 255.0f, 144.0f / 255.0f, 255.0f / 255.0f, 1.0f};
  }

  static constexpr Color Firebrick() {
    return {178.0f / 255.0f, 34.0f / 255.0f, 34.0f / 255.0f, 1.0f};
  }

  static constexpr Color FloralWhite() {
    return {255.0f / 255.0f, 250.0f / 255.0f, 240.0f / 255.0f, 1.0f};
  }

  static constexpr Color ForestGreen() {
    return {34.0f / 255.0f, 139.0f / 255.0f, 34.0f / 255.0f, 1.0f};
  }

  static constexpr Color Fuchsia() {
    return {255.0f / 255.0f, 0.0f / 255.0f, 255.0f / 255.0f, 1.0f};
  }

  static constexpr Color Gainsboro() {
    return {220.0f / 255.0f, 220.0f / 255.0f, 220.0f / 255.0f, 1.0f};
  }

  static constexpr Color Ghostwhite() {
    return {248.0f / 255.0f, 248.0f / 255.0f, 255.0f / 255.0f, 1.0f};
  }

  static constexpr Color Gold() {
    return {255.0f / 255.0f, 215.0f / 255.0f, 0.0f / 255.0f, 1.0f};
  }

  static constexpr Color Goldenrod() {
    return {218.0f / 255.0f, 165.0f / 255.0f, 32.0f / 255.0f, 1.0f};
  }

  static constexpr Color Gray() {
    return {128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1.0f};
  }

  static constexpr Color GreenYellow() {
    return {173.0f / 255.0f, 255.0f / 255.0f, 47.0f / 255.0f, 1.0f};
  }

  static constexpr Color Grey() {
    return {128.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1.0f};
  }

  static constexpr Color Honeydew() {
    return {240.0f / 255.0f, 255.0f / 255.0f, 240.0f / 255.0f, 1.0f};
  }

  static constexpr Color HotPink() {
    return {255.0f / 255.0f, 105.0f / 255.0f, 180.0f / 255.0f, 1.0f};
  }

  static constexpr Color IndianRed() {
    return {205.0f / 255.0f, 92.0f / 255.0f, 92.0f / 255.0f, 1.0f};
  }

  static constexpr Color Indigo() {
    return {75.0f / 255.0f, 0.0f / 255.0f, 130.0f / 255.0f, 1.0f};
  }

  static constexpr Color Ivory() {
    return {255.0f / 255.0f, 255.0f / 255.0f, 240.0f / 255.0f, 1.0f};
  }

  static constexpr Color Khaki() {
    return {240.0f / 255.0f, 230.0f / 255.0f, 140.0f / 255.0f, 1.0f};
  }

  static constexpr Color Lavender() {
    return {230.0f / 255.0f, 230.0f / 255.0f, 250.0f / 255.0f, 1.0f};
  }

  static constexpr Color LavenderBlush() {
    return {255.0f / 255.0f, 240.0f / 255.0f, 245.0f / 255.0f, 1.0f};
  }

  static constexpr Color LawnGreen() {
    return {124.0f / 255.0f, 252.0f / 255.0f, 0.0f / 255.0f, 1.0f};
  }

  static constexpr Color LemonChiffon() {
    return {255.0f / 255.0f, 250.0f / 255.0f, 205.0f / 255.0f, 1.0f};
  }

  static constexpr Color LightBlue() {
    return {173.0f / 255.0f, 216.0f / 255.0f, 230.0f / 255.0f, 1.0f};
  }

  static constexpr Color LightCoral() {
    return {240.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1.0f};
  }

  static constexpr Color LightCyan() {
    return {224.0f / 255.0f, 255.0f / 255.0f, 255.0f / 255.0f, 1.0f};
  }

  static constexpr Color LightGoldenrodYellow() {
    return {50.0f / 255.0f, 250.0f / 255.0f, 210.0f / 255.0f, 1.0f};
  }

  static constexpr Color LightGray() {
    return {211.0f / 255.0f, 211.0f / 255.0f, 211.0f / 255.0f, 1.0f};
  }

  static constexpr Color LightGreen() {
    return {144.0f / 255.0f, 238.0f / 255.0f, 144.0f / 255.0f, 1.0f};
  }

  static constexpr Color LightGrey() {
    return {211.0f / 255.0f, 211.0f / 255.0f, 211.0f / 255.0f, 1.0f};
  }

  static constexpr Color LightPink() {
    return {255.0f / 255.0f, 182.0f / 255.0f, 193.0f / 255.0f, 1.0f};
  }

  static constexpr Color LightSalmon() {
    return {255.0f / 255.0f, 160.0f / 255.0f, 122.0f / 255.0f, 1.0f};
  }

  static constexpr Color LightSeaGreen() {
    return {32.0f / 255.0f, 178.0f / 255.0f, 170.0f / 255.0f, 1.0f};
  }

  static constexpr Color LightSkyBlue() {
    return {135.0f / 255.0f, 206.0f / 255.0f, 250.0f / 255.0f, 1.0f};
  }

  static constexpr Color LightSlateGray() {
    return {119.0f / 255.0f, 136.0f / 255.0f, 153.0f / 255.0f, 1.0f};
  }

  static constexpr Color LightSlateGrey() {
    return {119.0f / 255.0f, 136.0f / 255.0f, 153.0f / 255.0f, 1.0f};
  }

  static constexpr Color LightSteelBlue() {
    return {176.0f / 255.0f, 196.0f / 255.0f, 222.0f / 255.0f, 1.0f};
  }

  static constexpr Color LightYellow() {
    return {255.0f / 255.0f, 255.0f / 255.0f, 224.0f / 255.0f, 1.0f};
  }

  static constexpr Color Lime() {
    return {0.0f / 255.0f, 255.0f / 255.0f, 0.0f / 255.0f, 1.0f};
  }

  static constexpr Color LimeGreen() {
    return {50.0f / 255.0f, 205.0f / 255.0f, 50.0f / 255.0f, 1.0f};
  }

  static constexpr Color Linen() {
    return {250.0f / 255.0f, 240.0f / 255.0f, 230.0f / 255.0f, 1.0f};
  }

  static constexpr Color Magenta() {
    return {255.0f / 255.0f, 0.0f / 255.0f, 255.0f / 255.0f, 1.0f};
  }

  static constexpr Color Maroon() {
    return {128.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f, 1.0f};
  }

  static constexpr Color MediumAquamarine() {
    return {102.0f / 255.0f, 205.0f / 255.0f, 170.0f / 255.0f, 1.0f};
  }

  static constexpr Color MediumBlue() {
    return {0.0f / 255.0f, 0.0f / 255.0f, 205.0f / 255.0f, 1.0f};
  }

  static constexpr Color MediumOrchid() {
    return {186.0f / 255.0f, 85.0f / 255.0f, 211.0f / 255.0f, 1.0f};
  }

  static constexpr Color MediumPurple() {
    return {147.0f / 255.0f, 112.0f / 255.0f, 219.0f / 255.0f, 1.0f};
  }

  static constexpr Color MediumSeagreen() {
    return {60.0f / 255.0f, 179.0f / 255.0f, 113.0f / 255.0f, 1.0f};
  }

  static constexpr Color MediumSlateBlue() {
    return {123.0f / 255.0f, 104.0f / 255.0f, 238.0f / 255.0f, 1.0f};
  }

  static constexpr Color MediumSpringGreen() {
    return {0.0f / 255.0f, 250.0f / 255.0f, 154.0f / 255.0f, 1.0f};
  }

  static constexpr Color MediumTurquoise() {
    return {72.0f / 255.0f, 209.0f / 255.0f, 204.0f / 255.0f, 1.0f};
  }

  static constexpr Color MediumVioletRed() {
    return {199.0f / 255.0f, 21.0f / 255.0f, 133.0f / 255.0f, 1.0f};
  }

  static constexpr Color MidnightBlue() {
    return {25.0f / 255.0f, 25.0f / 255.0f, 112.0f / 255.0f, 1.0f};
  }

  static constexpr Color MintCream() {
    return {245.0f / 255.0f, 255.0f / 255.0f, 250.0f / 255.0f, 1.0f};
  }

  static constexpr Color MistyRose() {
    return {255.0f / 255.0f, 228.0f / 255.0f, 225.0f / 255.0f, 1.0f};
  }

  static constexpr Color Moccasin() {
    return {255.0f / 255.0f, 228.0f / 255.0f, 181.0f / 255.0f, 1.0f};
  }

  static constexpr Color NavajoWhite() {
    return {255.0f / 255.0f, 222.0f / 255.0f, 173.0f / 255.0f, 1.0f};
  }

  static constexpr Color Navy() {
    return {0.0f / 255.0f, 0.0f / 255.0f, 128.0f / 255.0f, 1.0f};
  }

  static constexpr Color OldLace() {
    return {253.0f / 255.0f, 245.0f / 255.0f, 230.0f / 255.0f, 1.0f};
  }

  static constexpr Color Olive() {
    return {128.0f / 255.0f, 128.0f / 255.0f, 0.0f / 255.0f, 1.0f};
  }

  static constexpr Color OliveDrab() {
    return {107.0f / 255.0f, 142.0f / 255.0f, 35.0f / 255.0f, 1.0f};
  }

  static constexpr Color Orange() {
    return {255.0f / 255.0f, 165.0f / 255.0f, 0.0f / 255.0f, 1.0f};
  }

  static constexpr Color OrangeRed() {
    return {255.0f / 255.0f, 69.0f / 255.0f, 0.0f / 255.0f, 1.0f};
  }

  static constexpr Color Orchid() {
    return {218.0f / 255.0f, 112.0f / 255.0f, 214.0f / 255.0f, 1.0f};
  }

  static constexpr Color PaleGoldenrod() {
    return {238.0f / 255.0f, 232.0f / 255.0f, 170.0f / 255.0f, 1.0f};
  }

  static constexpr Color PaleGreen() {
    return {152.0f / 255.0f, 251.0f / 255.0f, 152.0f / 255.0f, 1.0f};
  }

  static constexpr Color PaleTurquoise() {
    return {175.0f / 255.0f, 238.0f / 255.0f, 238.0f / 255.0f, 1.0f};
  }

  static constexpr Color PaleVioletRed() {
    return {219.0f / 255.0f, 112.0f / 255.0f, 147.0f / 255.0f, 1.0f};
  }

  static constexpr Color PapayaWhip() {
    return {255.0f / 255.0f, 239.0f / 255.0f, 213.0f / 255.0f, 1.0f};
  }

  static constexpr Color Peachpuff() {
    return {255.0f / 255.0f, 218.0f / 255.0f, 185.0f / 255.0f, 1.0f};
  }

  static constexpr Color Peru() {
    return {205.0f / 255.0f, 133.0f / 255.0f, 63.0f / 255.0f, 1.0f};
  }

  static constexpr Color Pink() {
    return {255.0f / 255.0f, 192.0f / 255.0f, 203.0f / 255.0f, 1.0f};
  }

  static constexpr Color Plum() {
    return {221.0f / 255.0f, 160.0f / 255.0f, 221.0f / 255.0f, 1.0f};
  }

  static constexpr Color PowderBlue() {
    return {176.0f / 255.0f, 224.0f / 255.0f, 230.0f / 255.0f, 1.0f};
  }

  static constexpr Color Purple() {
    return {128.0f / 255.0f, 0.0f / 255.0f, 128.0f / 255.0f, 1.0f};
  }

  static constexpr Color RosyBrown() {
    return {188.0f / 255.0f, 143.0f / 255.0f, 143.0f / 255.0f, 1.0f};
  }

  static constexpr Color RoyalBlue() {
    return {65.0f / 255.0f, 105.0f / 255.0f, 225.0f / 255.0f, 1.0f};
  }

  static constexpr Color SaddleBrown() {
    return {139.0f / 255.0f, 69.0f / 255.0f, 19.0f / 255.0f, 1.0f};
  }

  static constexpr Color Salmon() {
    return {250.0f / 255.0f, 128.0f / 255.0f, 114.0f / 255.0f, 1.0f};
  }

  static constexpr Color SandyBrown() {
    return {244.0f / 255.0f, 164.0f / 255.0f, 96.0f / 255.0f, 1.0f};
  }

  static constexpr Color Seagreen() {
    return {46.0f / 255.0f, 139.0f / 255.0f, 87.0f / 255.0f, 1.0f};
  }

  static constexpr Color Seashell() {
    return {255.0f / 255.0f, 245.0f / 255.0f, 238.0f / 255.0f, 1.0f};
  }

  static constexpr Color Sienna() {
    return {160.0f / 255.0f, 82.0f / 255.0f, 45.0f / 255.0f, 1.0f};
  }

  static constexpr Color Silver() {
    return {192.0f / 255.0f, 192.0f / 255.0f, 192.0f / 255.0f, 1.0f};
  }

  static constexpr Color SkyBlue() {
    return {135.0f / 255.0f, 206.0f / 255.0f, 235.0f / 255.0f, 1.0f};
  }

  static constexpr Color SlateBlue() {
    return {106.0f / 255.0f, 90.0f / 255.0f, 205.0f / 255.0f, 1.0f};
  }

  static constexpr Color SlateGray() {
    return {112.0f / 255.0f, 128.0f / 255.0f, 144.0f / 255.0f, 1.0f};
  }

  static constexpr Color SlateGrey() {
    return {112.0f / 255.0f, 128.0f / 255.0f, 144.0f / 255.0f, 1.0f};
  }

  static constexpr Color Snow() {
    return {255.0f / 255.0f, 250.0f / 255.0f, 250.0f / 255.0f, 1.0f};
  }

  static constexpr Color SpringGreen() {
    return {0.0f / 255.0f, 255.0f / 255.0f, 127.0f / 255.0f, 1.0f};
  }

  static constexpr Color SteelBlue() {
    return {70.0f / 255.0f, 130.0f / 255.0f, 180.0f / 255.0f, 1.0f};
  }

  static constexpr Color Tan() {
    return {210.0f / 255.0f, 180.0f / 255.0f, 140.0f / 255.0f, 1.0f};
  }

  static constexpr Color Teal() {
    return {0.0f / 255.0f, 128.0f / 255.0f, 128.0f / 255.0f, 1.0f};
  }

  static constexpr Color Thistle() {
    return {216.0f / 255.0f, 191.0f / 255.0f, 216.0f / 255.0f, 1.0f};
  }

  static constexpr Color Tomato() {
    return {255.0f / 255.0f, 99.0f / 255.0f, 71.0f / 255.0f, 1.0f};
  }

  static constexpr Color Turquoise() {
    return {64.0f / 255.0f, 224.0f / 255.0f, 208.0f / 255.0f, 1.0f};
  }

  static constexpr Color Violet() {
    return {238.0f / 255.0f, 130.0f / 255.0f, 238.0f / 255.0f, 1.0f};
  }

  static constexpr Color Wheat() {
    return {245.0f / 255.0f, 222.0f / 255.0f, 179.0f / 255.0f, 1.0f};
  }

  static constexpr Color Whitesmoke() {
    return {245.0f / 255.0f, 245.0f / 255.0f, 245.0f / 255.0f, 1.0f};
  }

  static constexpr Color Yellow() {
    return {255.0f / 255.0f, 255.0f / 255.0f, 0.0f / 255.0f, 1.0f};
  }

  static constexpr Color YellowGreen() {
    return {154.0f / 255.0f, 205.0f / 255.0f, 50.0f / 255.0f, 1.0f};
  }

  static Color Random() {
    return {
        static_cast<Scalar>((std::rand() % 255) / 255.0f),  //
        static_cast<Scalar>((std::rand() % 255) / 255.0f),  //
        static_cast<Scalar>((std::rand() % 255) / 255.0f),  //
        1.0f                                                //
    };
  }

  static Color BlendColor(const Color& src,
                          const Color& dst,
                          BlendMode blend_mode);

  Color operator*(const Color& c) const {
    return Color(red * c.red, green * c.green, blue * c.blue, alpha * c.alpha);
  }

  Color operator+(const Color& c) const;

  Color operator-(const Color& c) const;

  Color operator*(Scalar value) const;

  constexpr bool IsTransparent() const { return alpha == 0.0f; }

  constexpr bool IsOpaque() const { return alpha == 1.0f; }
};

/**
 *  Represents a color by its constituent hue, saturation, brightness and alpha
 */
struct ColorHSB {
  /**
   *  The hue of the color (0 to 1)
   */
  Scalar hue;

  /**
   *  The saturation of the color (0 to 1)
   */
  Scalar saturation;

  /**
   *  The brightness of the color (0 to 1)
   */
  Scalar brightness;

  /**
   *  The alpha of the color (0 to 1)
   */
  Scalar alpha;

  constexpr ColorHSB(Scalar h, Scalar s, Scalar b, Scalar a)
      : hue(h), saturation(s), brightness(b), alpha(a) {}

  static ColorHSB FromRGB(Color rgb);

  Color ToRGBA() const;
};

static_assert(sizeof(Color) == 4 * sizeof(Scalar));

}  // namespace impeller

namespace std {

inline std::ostream& operator<<(std::ostream& out, const impeller::Color& c) {
  out << "(" << c.red << ", " << c.green << ", " << c.blue << ", " << c.alpha
      << ")";
  return out;
}

}  // namespace std
