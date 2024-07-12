// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "flutter/testing/testing.h"
#include "impeller/aiks/aiks_playground.h"
#include "impeller/aiks/canvas.h"
#include "impeller/aiks/image.h"
#include "impeller/aiks/paint_pass_delegate.h"
#include "impeller/entity/contents/color_source_contents.h"
#include "impeller/entity/contents/filters/inputs/filter_input.h"
#include "impeller/entity/contents/runtime_effect_contents.h"
#include "impeller/entity/contents/scene_contents.h"
#include "impeller/entity/contents/solid_color_contents.h"
#include "impeller/entity/contents/tiled_texture_contents.h"
#include "impeller/geometry/color.h"
#include "impeller/geometry/constants.h"
#include "impeller/geometry/geometry_asserts.h"
#include "impeller/geometry/matrix.h"
#include "impeller/geometry/path_builder.h"
#include "impeller/golden_tests/golden_playground_test.h"
#include "impeller/playground/widgets.h"
#include "impeller/renderer/command_buffer.h"
#include "impeller/renderer/snapshot.h"
#include "impeller/scene/material.h"
#include "impeller/scene/node.h"
#include "impeller/typographer/backends/skia/text_frame_skia.h"
#include "impeller/typographer/backends/skia/text_render_context_skia.h"
#include "third_party/imgui/imgui.h"
#include "third_party/skia/include/core/SkData.h"

namespace impeller {
namespace testing {

#ifdef IMPELLER_GOLDEN_TESTS
using AiksTest = GoldenPlaygroundTest;
#else
using AiksTest = AiksPlayground;
#endif
INSTANTIATE_PLAYGROUND_SUITE(AiksTest);

TEST_P(AiksTest, RotateColorFilteredPath) {
  Canvas canvas;
  canvas.Concat(Matrix::MakeTranslation({300, 300}));
  canvas.Concat(Matrix::MakeRotationZ(Radians(kPiOver2)));
  auto arrow_stem =
      PathBuilder{}.MoveTo({120, 190}).LineTo({120, 50}).TakePath();
  auto arrow_head = PathBuilder{}
                        .MoveTo({50, 120})
                        .LineTo({120, 190})
                        .LineTo({190, 120})
                        .TakePath();
  auto paint = Paint{
      .stroke_width = 15.0,
      .stroke_cap = Cap::kRound,
      .stroke_join = Join::kRound,
      .style = Paint::Style::kStroke,
      .color_filter =
          [](FilterInput::Ref input) {
            return ColorFilterContents::MakeBlend(
                BlendMode::kSourceIn, {std::move(input)}, Color::AliceBlue());
          },
  };

  canvas.DrawPath(arrow_stem, paint);
  canvas.DrawPath(arrow_head, paint);
  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanvasCTMCanBeUpdated) {
  Canvas canvas;
  Matrix identity;
  ASSERT_MATRIX_NEAR(canvas.GetCurrentTransformation(), identity);
  canvas.Translate(Size{100, 100});
  ASSERT_MATRIX_NEAR(canvas.GetCurrentTransformation(),
                     Matrix::MakeTranslation({100.0, 100.0, 0.0}));
}

TEST_P(AiksTest, CanvasCanPushPopCTM) {
  Canvas canvas;
  ASSERT_EQ(canvas.GetSaveCount(), 1u);
  ASSERT_EQ(canvas.Restore(), false);

  canvas.Translate(Size{100, 100});
  canvas.Save();
  ASSERT_EQ(canvas.GetSaveCount(), 2u);
  ASSERT_MATRIX_NEAR(canvas.GetCurrentTransformation(),
                     Matrix::MakeTranslation({100.0, 100.0, 0.0}));
  ASSERT_TRUE(canvas.Restore());
  ASSERT_EQ(canvas.GetSaveCount(), 1u);
  ASSERT_MATRIX_NEAR(canvas.GetCurrentTransformation(),
                     Matrix::MakeTranslation({100.0, 100.0, 0.0}));
}

TEST_P(AiksTest, CanRenderColoredRect) {
  Canvas canvas;
  Paint paint;
  paint.color = Color::Blue();
  canvas.DrawPath(PathBuilder{}
                      .AddRect(Rect::MakeXYWH(100.0, 100.0, 100.0, 100.0))
                      .TakePath(),
                  paint);
  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanRenderImage) {
  Canvas canvas;
  Paint paint;
  auto image = std::make_shared<Image>(CreateTextureForFixture("kalimba.jpg"));
  paint.color = Color::Red();
  canvas.DrawImage(image, Point::MakeXY(100.0, 100.0), paint);
  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanRenderInvertedImage) {
  Canvas canvas;
  Paint paint;
  auto image = std::make_shared<Image>(CreateTextureForFixture("kalimba.jpg"));
  paint.color = Color::Red();
  paint.invert_colors = true;
  canvas.DrawImage(image, Point::MakeXY(100.0, 100.0), paint);
  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

namespace {
bool GenerateMipmap(const std::shared_ptr<Context>& context,
                    std::shared_ptr<Texture> texture,
                    std::string label) {
  auto buffer = context->CreateCommandBuffer();
  if (!buffer) {
    return false;
  }
  auto pass = buffer->CreateBlitPass();
  if (!pass) {
    return false;
  }
  pass->GenerateMipmap(std::move(texture), std::move(label));
  pass->EncodeCommands(context->GetResourceAllocator());
  return buffer->SubmitCommands();
}

void CanRenderTiledTexture(AiksTest* aiks_test, Entity::TileMode tile_mode) {
  auto context = aiks_test->GetContext();
  ASSERT_TRUE(context);
  auto texture = aiks_test->CreateTextureForFixture("table_mountain_nx.png",
                                                    /*enable_mipmapping=*/true);
  GenerateMipmap(context, texture, "table_mountain_nx");
  Canvas canvas;
  canvas.Scale(aiks_test->GetContentScale());
  canvas.Translate({100.0f, 100.0f, 0});
  Paint paint;
  paint.color_source = [texture, tile_mode]() {
    auto contents = std::make_shared<TiledTextureContents>();
    contents->SetTexture(texture);
    contents->SetTileModes(tile_mode, tile_mode);
    return contents;
  };
  paint.color = Color(1, 1, 1, 1);
  canvas.DrawRect({0, 0, 600, 600}, paint);

  // Should not change the image.
  constexpr auto stroke_width = 64;
  paint.style = Paint::Style::kStroke;
  paint.stroke_width = stroke_width;
  if (tile_mode == Entity::TileMode::kDecal) {
    canvas.DrawRect({stroke_width, stroke_width, 600, 600}, paint);
  } else {
    canvas.DrawRect({0, 0, 600, 600}, paint);
  }

  // Should not change the image.
  PathBuilder path_builder;
  path_builder.AddCircle({150, 150}, 150);
  path_builder.AddRoundedRect(Rect::MakeLTRB(300, 300, 600, 600), 10);
  paint.style = Paint::Style::kFill;
  canvas.DrawPath(path_builder.TakePath(), paint);

  ASSERT_TRUE(aiks_test->OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}
}  // namespace

TEST_P(AiksTest, CanRenderTiledTextureClamp) {
  CanRenderTiledTexture(this, Entity::TileMode::kClamp);
}

TEST_P(AiksTest, CanRenderTiledTextureRepeat) {
  CanRenderTiledTexture(this, Entity::TileMode::kRepeat);
}

TEST_P(AiksTest, CanRenderTiledTextureMirror) {
  CanRenderTiledTexture(this, Entity::TileMode::kMirror);
}

TEST_P(AiksTest, CanRenderTiledTextureDecal) {
  CanRenderTiledTexture(this, Entity::TileMode::kDecal);
}

TEST_P(AiksTest, CanRenderImageRect) {
  Canvas canvas;
  Paint paint;
  auto image = std::make_shared<Image>(CreateTextureForFixture("kalimba.jpg"));
  auto source_rect = Rect::MakeSize(Size(image->GetSize()));

  // Render the bottom right quarter of the source image in a stretched rect.
  source_rect.size.width /= 2;
  source_rect.size.height /= 2;
  source_rect.origin.x += source_rect.size.width;
  source_rect.origin.y += source_rect.size.height;
  canvas.DrawImageRect(image, source_rect, Rect::MakeXYWH(100, 100, 600, 600),
                       paint);
  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanRenderStrokes) {
  Canvas canvas;
  Paint paint;
  paint.color = Color::Red();
  paint.stroke_width = 20.0;
  paint.style = Paint::Style::kStroke;
  canvas.DrawPath(PathBuilder{}.AddLine({200, 100}, {800, 100}).TakePath(),
                  paint);
  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanRenderCurvedStrokes) {
  Canvas canvas;
  Paint paint;
  paint.color = Color::Red();
  paint.stroke_width = 25.0;
  paint.style = Paint::Style::kStroke;
  canvas.DrawPath(PathBuilder{}.AddCircle({500, 500}, 250).TakePath(), paint);
  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanRenderClips) {
  Canvas canvas;
  Paint paint;
  paint.color = Color::Fuchsia();
  canvas.ClipPath(
      PathBuilder{}.AddRect(Rect::MakeXYWH(0, 0, 500, 500)).TakePath());
  canvas.DrawPath(PathBuilder{}.AddCircle({500, 500}, 250).TakePath(), paint);
  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanRenderNestedClips) {
  Canvas canvas;
  Paint paint;
  paint.color = Color::Fuchsia();
  canvas.Save();
  canvas.ClipPath(PathBuilder{}.AddCircle({200, 400}, 300).TakePath());
  canvas.Restore();
  canvas.ClipPath(PathBuilder{}.AddCircle({600, 400}, 300).TakePath());
  canvas.ClipPath(PathBuilder{}.AddCircle({400, 600}, 300).TakePath());
  canvas.DrawRect(Rect::MakeXYWH(200, 200, 400, 400), paint);
  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanRenderDifferenceClips) {
  Paint paint;
  Canvas canvas;
  canvas.Translate({400, 400});

  // Limit drawing to face circle with a clip.
  canvas.ClipPath(PathBuilder{}.AddCircle(Point(), 200).TakePath());
  canvas.Save();

  // Cut away eyes/mouth using difference clips.
  canvas.ClipPath(PathBuilder{}.AddCircle({-100, -50}, 30).TakePath(),
                  Entity::ClipOperation::kDifference);
  canvas.ClipPath(PathBuilder{}.AddCircle({100, -50}, 30).TakePath(),
                  Entity::ClipOperation::kDifference);
  canvas.ClipPath(PathBuilder{}
                      .AddQuadraticCurve({-100, 50}, {0, 150}, {100, 50})
                      .TakePath(),
                  Entity::ClipOperation::kDifference);

  // Draw a huge yellow rectangle to prove the clipping works.
  paint.color = Color::Yellow();
  canvas.DrawRect(Rect::MakeXYWH(-1000, -1000, 2000, 2000), paint);

  // Remove the difference clips and draw hair that partially covers the eyes.
  canvas.Restore();
  paint.color = Color::Maroon();
  canvas.DrawPath(PathBuilder{}
                      .MoveTo({200, -200})
                      .HorizontalLineTo(-200)
                      .VerticalLineTo(-40)
                      .CubicCurveTo({0, -40}, {0, -80}, {200, -80})
                      .TakePath(),
                  paint);

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanRenderWithContiguousClipRestores) {
  Canvas canvas;

  // Cover the whole canvas with red.
  canvas.DrawPaint({.color = Color::Red()});

  canvas.Save();

  // Append two clips, the second resulting in empty coverage.
  canvas.ClipPath(
      PathBuilder{}.AddRect(Rect::MakeXYWH(100, 100, 100, 100)).TakePath());
  canvas.ClipPath(
      PathBuilder{}.AddRect(Rect::MakeXYWH(300, 300, 100, 100)).TakePath());

  // Restore to no clips.
  canvas.Restore();

  // Replace the whole canvas with green.
  canvas.DrawPaint({.color = Color::Green()});

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, ClipsUseCurrentTransform) {
  std::array<Color, 5> colors = {Color::White(), Color::Black(),
                                 Color::SkyBlue(), Color::Red(),
                                 Color::Yellow()};
  Canvas canvas;
  Paint paint;

  canvas.Translate(Vector3(300, 300));
  for (int i = 0; i < 15; i++) {
    canvas.Scale(Vector3(0.8, 0.8));

    paint.color = colors[i % colors.size()];
    canvas.ClipPath(PathBuilder{}.AddCircle({0, 0}, 300).TakePath());
    canvas.DrawRect(Rect(-300, -300, 600, 600), paint);
  }
  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanSaveLayerStandalone) {
  Canvas canvas;

  Paint red;
  red.color = Color::Red();

  Paint alpha;
  alpha.color = Color::Red().WithAlpha(0.5);

  canvas.SaveLayer(alpha);

  canvas.DrawCircle({125, 125}, 125, red);

  canvas.Restore();

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

namespace {
void CanRenderLinearGradient(AiksTest* aiks_test, Entity::TileMode tile_mode) {
  Canvas canvas;
  canvas.Scale(aiks_test->GetContentScale());
  Paint paint;
  canvas.Translate({100.0f, 0, 0});
  paint.color_source = [tile_mode]() {
    std::vector<Color> colors = {Color{0.9568, 0.2627, 0.2118, 1.0},
                                 Color{0.1294, 0.5882, 0.9529, 0.0}};
    std::vector<Scalar> stops = {0.0, 1.0};

    auto contents = std::make_shared<LinearGradientContents>();
    contents->SetEndPoints({0, 0}, {200, 200});
    contents->SetColors(std::move(colors));
    contents->SetStops(std::move(stops));
    contents->SetTileMode(tile_mode);
    return contents;
  };
  paint.color = Color(1.0, 1.0, 1.0, 1.0);
  canvas.DrawRect({0, 0, 600, 600}, paint);
  ASSERT_TRUE(aiks_test->OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}
}  // namespace

TEST_P(AiksTest, CanRenderLinearGradientClamp) {
  CanRenderLinearGradient(this, Entity::TileMode::kClamp);
}
TEST_P(AiksTest, CanRenderLinearGradientRepeat) {
  CanRenderLinearGradient(this, Entity::TileMode::kRepeat);
}
TEST_P(AiksTest, CanRenderLinearGradientMirror) {
  CanRenderLinearGradient(this, Entity::TileMode::kMirror);
}
TEST_P(AiksTest, CanRenderLinearGradientDecal) {
  CanRenderLinearGradient(this, Entity::TileMode::kDecal);
}

namespace {
void CanRenderLinearGradientWithOverlappingStops(AiksTest* aiks_test,
                                                 Entity::TileMode tile_mode) {
  Canvas canvas;
  Paint paint;
  canvas.Translate({100.0, 100.0, 0});
  paint.color_source = [tile_mode]() {
    std::vector<Color> colors = {
        Color{0.9568, 0.2627, 0.2118, 1.0}, Color{0.9568, 0.2627, 0.2118, 1.0},
        Color{0.1294, 0.5882, 0.9529, 1.0}, Color{0.1294, 0.5882, 0.9529, 1.0}};
    std::vector<Scalar> stops = {0.0, 0.5, 0.5, 1.0};

    auto contents = std::make_shared<LinearGradientContents>();
    contents->SetEndPoints({0, 0}, {500, 500});
    contents->SetColors(std::move(colors));
    contents->SetStops(std::move(stops));
    contents->SetTileMode(tile_mode);
    return contents;
  };
  paint.color = Color(1.0, 1.0, 1.0, 1.0);
  canvas.DrawRect({0, 0, 500, 500}, paint);
  ASSERT_TRUE(aiks_test->OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}
}  // namespace

// Only clamp is necessary. All tile modes are the same output.
TEST_P(AiksTest, CanRenderLinearGradientWithOverlappingStopsClamp) {
  CanRenderLinearGradientWithOverlappingStops(this, Entity::TileMode::kClamp);
}

namespace {
void CanRenderLinearGradientManyColors(AiksTest* aiks_test,
                                       Entity::TileMode tile_mode) {
  Canvas canvas;
  canvas.Scale(aiks_test->GetContentScale());
  Paint paint;
  canvas.Translate({100, 100, 0});
  paint.color_source = [tile_mode]() {
    std::vector<Color> colors = {
        Color{0x1f / 255.0, 0.0, 0x5c / 255.0, 1.0},
        Color{0x5b / 255.0, 0.0, 0x60 / 255.0, 1.0},
        Color{0x87 / 255.0, 0x01 / 255.0, 0x60 / 255.0, 1.0},
        Color{0xac / 255.0, 0x25 / 255.0, 0x53 / 255.0, 1.0},
        Color{0xe1 / 255.0, 0x6b / 255.0, 0x5c / 255.0, 1.0},
        Color{0xf3 / 255.0, 0x90 / 255.0, 0x60 / 255.0, 1.0},
        Color{0xff / 255.0, 0xb5 / 255.0, 0x6b / 250.0, 1.0}};
    std::vector<Scalar> stops = {
        0.0,
        (1.0 / 6.0) * 1,
        (1.0 / 6.0) * 2,
        (1.0 / 6.0) * 3,
        (1.0 / 6.0) * 4,
        (1.0 / 6.0) * 5,
        1.0,
    };

    auto contents = std::make_shared<LinearGradientContents>();
    contents->SetEndPoints({0, 0}, {200, 200});
    contents->SetColors(std::move(colors));
    contents->SetStops(std::move(stops));
    contents->SetTileMode(tile_mode);
    return contents;
  };
  paint.color = Color(1.0, 1.0, 1.0, 1.0);
  canvas.DrawRect({0, 0, 600, 600}, paint);
  canvas.Restore();
  ASSERT_TRUE(aiks_test->OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}
}  // namespace

TEST_P(AiksTest, CanRenderLinearGradientManyColorsClamp) {
  CanRenderLinearGradientManyColors(this, Entity::TileMode::kClamp);
}
TEST_P(AiksTest, CanRenderLinearGradientManyColorsRepeat) {
  CanRenderLinearGradientManyColors(this, Entity::TileMode::kRepeat);
}
TEST_P(AiksTest, CanRenderLinearGradientManyColorsMirror) {
  CanRenderLinearGradientManyColors(this, Entity::TileMode::kMirror);
}
TEST_P(AiksTest, CanRenderLinearGradientManyColorsDecal) {
  CanRenderLinearGradientManyColors(this, Entity::TileMode::kDecal);
}

namespace {
void CanRenderLinearGradientWayManyColors(AiksTest* aiks_test,
                                          Entity::TileMode tile_mode) {
  Canvas canvas;
  Paint paint;
  canvas.Translate({100.0, 100.0, 0});
  auto color = Color{0x1f / 255.0, 0.0, 0x5c / 255.0, 1.0};
  std::vector<Color> colors;
  std::vector<Scalar> stops;
  auto current_stop = 0.0;
  for (int i = 0; i < 2000; i++) {
    colors.push_back(color);
    stops.push_back(current_stop);
    current_stop += 1 / 2000.0;
  }
  stops[2000 - 1] = 1.0;
  paint.color_source = [tile_mode, stops = std::move(stops),
                        colors = std::move(colors)]() {
    auto contents = std::make_shared<LinearGradientContents>();
    contents->SetEndPoints({0, 0}, {200, 200});
    contents->SetColors(colors);
    contents->SetStops(stops);
    contents->SetTileMode(tile_mode);
    return contents;
  };
  canvas.DrawRect({0, 0, 600, 600}, paint);
  ASSERT_TRUE(aiks_test->OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}
}  // namespace

// Only test clamp on purpose since they all look the same.
TEST_P(AiksTest, CanRenderLinearGradientWayManyColorsClamp) {
  CanRenderLinearGradientWayManyColors(this, Entity::TileMode::kClamp);
}

TEST_P(AiksTest, CanRenderLinearGradientManyColorsUnevenStops) {
  auto callback = [&](AiksContext& renderer, RenderTarget& render_target) {
    const char* tile_mode_names[] = {"Clamp", "Repeat", "Mirror", "Decal"};
    const Entity::TileMode tile_modes[] = {
        Entity::TileMode::kClamp, Entity::TileMode::kRepeat,
        Entity::TileMode::kMirror, Entity::TileMode::kDecal};

    static int selected_tile_mode = 0;
    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Combo("Tile mode", &selected_tile_mode, tile_mode_names,
                 sizeof(tile_mode_names) / sizeof(char*));
    static Matrix matrix = {
        1, 0, 0, 0,  //
        0, 1, 0, 0,  //
        0, 0, 1, 0,  //
        0, 0, 0, 1   //
    };
    std::string label = "##1";
    for (int i = 0; i < 4; i++) {
      ImGui::InputScalarN(label.c_str(), ImGuiDataType_Float, &(matrix.vec[i]),
                          4, NULL, NULL, "%.2f", 0);
      label[2]++;
    }
    ImGui::End();

    Canvas canvas;
    Paint paint;
    canvas.Translate({100.0, 100.0, 0});
    auto tile_mode = tile_modes[selected_tile_mode];
    paint.color_source = [tile_mode]() {
      std::vector<Color> colors = {
          Color{0x1f / 255.0, 0.0, 0x5c / 255.0, 1.0},
          Color{0x5b / 255.0, 0.0, 0x60 / 255.0, 1.0},
          Color{0x87 / 255.0, 0x01 / 255.0, 0x60 / 255.0, 1.0},
          Color{0xac / 255.0, 0x25 / 255.0, 0x53 / 255.0, 1.0},
          Color{0xe1 / 255.0, 0x6b / 255.0, 0x5c / 255.0, 1.0},
          Color{0xf3 / 255.0, 0x90 / 255.0, 0x60 / 255.0, 1.0},
          Color{0xff / 255.0, 0xb5 / 255.0, 0x6b / 250.0, 1.0}};
      std::vector<Scalar> stops = {
          0.0,         2.0 / 62.0,  4.0 / 62.0, 8.0 / 62.0,
          16.0 / 62.0, 32.0 / 62.0, 1.0,
      };

      auto contents = std::make_shared<LinearGradientContents>();
      contents->SetEndPoints({0, 0}, {200, 200});
      contents->SetColors(std::move(colors));
      contents->SetStops(std::move(stops));
      contents->SetTileMode(tile_mode);
      contents->SetEffectTransform(matrix);
      return contents;
    };
    canvas.DrawRect({0, 0, 600, 600}, paint);
    return renderer.Render(canvas.EndRecordingAsPicture(), render_target);
  };
  ASSERT_TRUE(OpenPlaygroundHere(callback));
}

TEST_P(AiksTest, CanRenderRadialGradient) {
  auto callback = [&](AiksContext& renderer, RenderTarget& render_target) {
    const char* tile_mode_names[] = {"Clamp", "Repeat", "Mirror", "Decal"};
    const Entity::TileMode tile_modes[] = {
        Entity::TileMode::kClamp, Entity::TileMode::kRepeat,
        Entity::TileMode::kMirror, Entity::TileMode::kDecal};

    static int selected_tile_mode = 0;
    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Combo("Tile mode", &selected_tile_mode, tile_mode_names,
                 sizeof(tile_mode_names) / sizeof(char*));
    static Matrix matrix = {
        1, 0, 0, 0,  //
        0, 1, 0, 0,  //
        0, 0, 1, 0,  //
        0, 0, 0, 1   //
    };
    std::string label = "##1";
    for (int i = 0; i < 4; i++) {
      ImGui::InputScalarN(label.c_str(), ImGuiDataType_Float, &(matrix.vec[i]),
                          4, NULL, NULL, "%.2f", 0);
      label[2]++;
    }
    ImGui::End();

    Canvas canvas;
    Paint paint;
    canvas.Translate({100.0, 100.0, 0});
    auto tile_mode = tile_modes[selected_tile_mode];
    paint.color_source = [tile_mode]() {
      std::vector<Color> colors = {Color{0.9568, 0.2627, 0.2118, 1.0},
                                   Color{0.1294, 0.5882, 0.9529, 1.0}};
      std::vector<Scalar> stops = {0.0, 1.0};

      auto contents = std::make_shared<RadialGradientContents>();
      contents->SetCenterAndRadius({100, 100}, 100);
      contents->SetColors(std::move(colors));
      contents->SetStops(std::move(stops));
      contents->SetTileMode(tile_mode);
      contents->SetEffectTransform(matrix);
      return contents;
    };
    canvas.DrawRect({0, 0, 600, 600}, paint);
    return renderer.Render(canvas.EndRecordingAsPicture(), render_target);
  };
  ASSERT_TRUE(OpenPlaygroundHere(callback));
}

TEST_P(AiksTest, CanRenderRadialGradientManyColors) {
  auto callback = [&](AiksContext& renderer, RenderTarget& render_target) {
    const char* tile_mode_names[] = {"Clamp", "Repeat", "Mirror", "Decal"};
    const Entity::TileMode tile_modes[] = {
        Entity::TileMode::kClamp, Entity::TileMode::kRepeat,
        Entity::TileMode::kMirror, Entity::TileMode::kDecal};

    static int selected_tile_mode = 0;
    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Combo("Tile mode", &selected_tile_mode, tile_mode_names,
                 sizeof(tile_mode_names) / sizeof(char*));
    static Matrix matrix = {
        1, 0, 0, 0,  //
        0, 1, 0, 0,  //
        0, 0, 1, 0,  //
        0, 0, 0, 1   //
    };
    std::string label = "##1";
    for (int i = 0; i < 4; i++) {
      ImGui::InputScalarN(label.c_str(), ImGuiDataType_Float, &(matrix.vec[i]),
                          4, NULL, NULL, "%.2f", 0);
      label[2]++;
    }
    ImGui::End();

    Canvas canvas;
    Paint paint;
    canvas.Translate({100.0, 100.0, 0});
    auto tile_mode = tile_modes[selected_tile_mode];
    paint.color_source = [tile_mode]() {
      std::vector<Color> colors = {
          Color{0x1f / 255.0, 0.0, 0x5c / 255.0, 1.0},
          Color{0x5b / 255.0, 0.0, 0x60 / 255.0, 1.0},
          Color{0x87 / 255.0, 0x01 / 255.0, 0x60 / 255.0, 1.0},
          Color{0xac / 255.0, 0x25 / 255.0, 0x53 / 255.0, 1.0},
          Color{0xe1 / 255.0, 0x6b / 255.0, 0x5c / 255.0, 1.0},
          Color{0xf3 / 255.0, 0x90 / 255.0, 0x60 / 255.0, 1.0},
          Color{0xff / 255.0, 0xb5 / 255.0, 0x6b / 250.0, 1.0}};
      std::vector<Scalar> stops = {
          0.0,
          (1.0 / 6.0) * 1,
          (1.0 / 6.0) * 2,
          (1.0 / 6.0) * 3,
          (1.0 / 6.0) * 4,
          (1.0 / 6.0) * 5,
          1.0,
      };

      auto contents = std::make_shared<RadialGradientContents>();
      contents->SetCenterAndRadius({100, 100}, 100);
      contents->SetColors(std::move(colors));
      contents->SetStops(std::move(stops));
      contents->SetTileMode(tile_mode);
      contents->SetEffectTransform(matrix);
      return contents;
    };
    canvas.DrawRect({0, 0, 600, 600}, paint);
    return renderer.Render(canvas.EndRecordingAsPicture(), render_target);
  };
  ASSERT_TRUE(OpenPlaygroundHere(callback));
}

namespace {
void CanRenderSweepGradient(AiksTest* aiks_test, Entity::TileMode tile_mode) {
  Canvas canvas;
  canvas.Scale(aiks_test->GetContentScale());
  Paint paint;
  canvas.Translate({100, 100, 0});
  paint.color_source = [tile_mode]() {
    auto contents = std::make_shared<SweepGradientContents>();
    contents->SetCenterAndAngles({100, 100}, Degrees(45), Degrees(135));
    std::vector<Color> colors = {Color{0.9568, 0.2627, 0.2118, 1.0},
                                 Color{0.1294, 0.5882, 0.9529, 1.0}};
    std::vector<Scalar> stops = {0.0, 1.0};
    contents->SetColors(std::move(colors));
    contents->SetStops(std::move(stops));
    contents->SetTileMode(tile_mode);
    return contents;
  };
  canvas.DrawRect({0, 0, 600, 600}, paint);
  ASSERT_TRUE(aiks_test->OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}
}  // namespace

TEST_P(AiksTest, CanRenderSweepGradientClamp) {
  CanRenderSweepGradient(this, Entity::TileMode::kClamp);
}
TEST_P(AiksTest, CanRenderSweepGradientRepeat) {
  CanRenderSweepGradient(this, Entity::TileMode::kRepeat);
}
TEST_P(AiksTest, CanRenderSweepGradientMirror) {
  CanRenderSweepGradient(this, Entity::TileMode::kMirror);
}
TEST_P(AiksTest, CanRenderSweepGradientDecal) {
  CanRenderSweepGradient(this, Entity::TileMode::kDecal);
}

namespace {
void CanRenderSweepGradientManyColors(AiksTest* aiks_test,
                                      Entity::TileMode tile_mode) {
  Canvas canvas;
  Paint paint;
  canvas.Translate({100.0, 100.0, 0});
  paint.color_source = [tile_mode]() {
    auto contents = std::make_shared<SweepGradientContents>();
    contents->SetCenterAndAngles({100, 100}, Degrees(45), Degrees(135));
    std::vector<Color> colors = {
        Color{0x1f / 255.0, 0.0, 0x5c / 255.0, 1.0},
        Color{0x5b / 255.0, 0.0, 0x60 / 255.0, 1.0},
        Color{0x87 / 255.0, 0x01 / 255.0, 0x60 / 255.0, 1.0},
        Color{0xac / 255.0, 0x25 / 255.0, 0x53 / 255.0, 1.0},
        Color{0xe1 / 255.0, 0x6b / 255.0, 0x5c / 255.0, 1.0},
        Color{0xf3 / 255.0, 0x90 / 255.0, 0x60 / 255.0, 1.0},
        Color{0xff / 255.0, 0xb5 / 255.0, 0x6b / 250.0, 1.0}};
    std::vector<Scalar> stops = {
        0.0,
        (1.0 / 6.0) * 1,
        (1.0 / 6.0) * 2,
        (1.0 / 6.0) * 3,
        (1.0 / 6.0) * 4,
        (1.0 / 6.0) * 5,
        1.0,
    };

    contents->SetStops(std::move(stops));
    contents->SetColors(std::move(colors));
    contents->SetTileMode(tile_mode);
    return contents;
  };
  canvas.DrawRect({0, 0, 600, 600}, paint);
  ASSERT_TRUE(aiks_test->OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}
}  // namespace

TEST_P(AiksTest, CanRenderSweepGradientManyColorsClamp) {
  CanRenderSweepGradientManyColors(this, Entity::TileMode::kClamp);
}
TEST_P(AiksTest, CanRenderSweepGradientManyColorsRepeat) {
  CanRenderSweepGradientManyColors(this, Entity::TileMode::kRepeat);
}
TEST_P(AiksTest, CanRenderSweepGradientManyColorsMirror) {
  CanRenderSweepGradientManyColors(this, Entity::TileMode::kMirror);
}
TEST_P(AiksTest, CanRenderSweepGradientManyColorsDecal) {
  CanRenderSweepGradientManyColors(this, Entity::TileMode::kDecal);
}

TEST_P(AiksTest, CanRenderDifferentShapesWithSameColorSource) {
  Canvas canvas;
  Paint paint;
  paint.color_source = []() {
    auto contents = std::make_shared<LinearGradientContents>();
    contents->SetEndPoints({0, 0}, {100, 100});
    std::vector<Color> colors = {Color{0.9568, 0.2627, 0.2118, 1.0},
                                 Color{0.1294, 0.5882, 0.9529, 1.0}};
    std::vector<Scalar> stops = {
        0.0,
        1.0,
    };
    contents->SetColors(std::move(colors));
    contents->SetStops(std::move(stops));
    contents->SetTileMode(Entity::TileMode::kRepeat);
    return contents;
  };
  canvas.Save();
  canvas.Translate({100, 100, 0});
  canvas.DrawRect({0, 0, 200, 200}, paint);
  canvas.Restore();

  canvas.Save();
  canvas.Translate({100, 400, 0});
  canvas.DrawCircle({100, 100}, 100, paint);
  canvas.Restore();
  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanPictureConvertToImage) {
  Canvas recorder_canvas;
  Paint paint;
  paint.color = Color{0.9568, 0.2627, 0.2118, 1.0};
  recorder_canvas.DrawRect({100.0, 100.0, 600, 600}, paint);
  paint.color = Color{0.1294, 0.5882, 0.9529, 1.0};
  recorder_canvas.DrawRect({200.0, 200.0, 600, 600}, paint);

  Canvas canvas;
  AiksContext renderer(GetContext());
  paint.color = Color::BlackTransparent();
  canvas.DrawPaint(paint);
  Picture picture = recorder_canvas.EndRecordingAsPicture();
  auto image = picture.ToImage(renderer, ISize{1000, 1000});
  if (image) {
    canvas.DrawImage(image, Point(), Paint());
    paint.color = Color{0.1, 0.1, 0.1, 0.2};
    canvas.DrawRect(Rect::MakeSize(ISize{1000, 1000}), paint);
  }

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, BlendModeShouldCoverWholeScreen) {
  Canvas canvas;
  Paint paint;

  paint.color = Color::Red();
  canvas.DrawPaint(paint);

  paint.blend_mode = BlendMode::kSourceOver;
  canvas.SaveLayer(paint);

  paint.color = Color::White();
  canvas.DrawRect({100, 100, 400, 400}, paint);

  paint.blend_mode = BlendMode::kSource;
  canvas.SaveLayer(paint);

  paint.color = Color::Blue();
  canvas.DrawRect({200, 200, 200, 200}, paint);

  canvas.Restore();
  canvas.Restore();

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanRenderGroupOpacity) {
  Canvas canvas;

  Paint red;
  red.color = Color::Red();
  Paint green;
  green.color = Color::Green().WithAlpha(0.5);
  Paint blue;
  blue.color = Color::Blue();

  Paint alpha;
  alpha.color = Color::Red().WithAlpha(0.5);

  canvas.SaveLayer(alpha);

  canvas.DrawRect({000, 000, 100, 100}, red);
  canvas.DrawRect({020, 020, 100, 100}, green);
  canvas.DrawRect({040, 040, 100, 100}, blue);

  canvas.Restore();

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CoordinateConversionsAreCorrect) {
  Canvas canvas;

  // Render a texture directly.
  {
    Paint paint;
    auto image =
        std::make_shared<Image>(CreateTextureForFixture("kalimba.jpg"));
    paint.color = Color::Red();

    canvas.Save();
    canvas.Translate({100, 200, 0});
    canvas.Scale(Vector2{0.5, 0.5});
    canvas.DrawImage(image, Point::MakeXY(100.0, 100.0), paint);
    canvas.Restore();
  }

  // Render an offscreen rendered texture.
  {
    Paint red;
    red.color = Color::Red();
    Paint green;
    green.color = Color::Green();
    Paint blue;
    blue.color = Color::Blue();

    Paint alpha;
    alpha.color = Color::Red().WithAlpha(0.5);

    canvas.SaveLayer(alpha);

    canvas.DrawRect({000, 000, 100, 100}, red);
    canvas.DrawRect({020, 020, 100, 100}, green);
    canvas.DrawRect({040, 040, 100, 100}, blue);

    canvas.Restore();
  }

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanPerformFullScreenMSAA) {
  Canvas canvas;

  Paint red;
  red.color = Color::Red();

  canvas.DrawCircle({250, 250}, 125, red);

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanPerformSkew) {
  Canvas canvas;

  Paint red;
  red.color = Color::Red();

  canvas.Skew(2, 5);
  canvas.DrawRect(Rect::MakeXYWH(0, 0, 100, 100), red);

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanPerformSaveLayerWithBounds) {
  Canvas canvas;

  Paint red;
  red.color = Color::Red();

  Paint green;
  green.color = Color::Green();

  Paint blue;
  blue.color = Color::Blue();

  Paint save;
  save.color = Color::Black();

  canvas.SaveLayer(save, Rect{0, 0, 50, 50});

  canvas.DrawRect({0, 0, 100, 100}, red);
  canvas.DrawRect({10, 10, 100, 100}, green);
  canvas.DrawRect({20, 20, 100, 100}, blue);

  canvas.Restore();

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest,
       CanPerformSaveLayerWithBoundsAndLargerIntermediateIsNotAllocated) {
  Canvas canvas;

  Paint red;
  red.color = Color::Red();

  Paint green;
  green.color = Color::Green();

  Paint blue;
  blue.color = Color::Blue();

  Paint save;
  save.color = Color::Black().WithAlpha(0.5);

  canvas.SaveLayer(save, Rect{0, 0, 100000, 100000});

  canvas.DrawRect({0, 0, 100, 100}, red);
  canvas.DrawRect({10, 10, 100, 100}, green);
  canvas.DrawRect({20, 20, 100, 100}, blue);

  canvas.Restore();

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanRenderRoundedRectWithNonUniformRadii) {
  Canvas canvas;

  Paint paint;
  paint.color = Color::Red();

  PathBuilder::RoundingRadii radii;
  radii.top_left = {50, 25};
  radii.top_right = {25, 50};
  radii.bottom_right = {50, 25};
  radii.bottom_left = {25, 50};

  auto path =
      PathBuilder{}.AddRoundedRect(Rect{100, 100, 500, 500}, radii).TakePath();

  canvas.DrawPath(path, paint);

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanRenderDifferencePaths) {
  Canvas canvas;

  Paint paint;
  paint.color = Color::Red();

  PathBuilder builder;

  PathBuilder::RoundingRadii radii;
  radii.top_left = {50, 25};
  radii.top_right = {25, 50};
  radii.bottom_right = {50, 25};
  radii.bottom_left = {25, 50};

  builder.AddRoundedRect({100, 100, 200, 200}, radii);
  builder.AddCircle({200, 200}, 50);
  auto path = builder.TakePath(FillType::kOdd);

  canvas.DrawImage(
      std::make_shared<Image>(CreateTextureForFixture("boston.jpg")), {10, 10},
      Paint{});
  canvas.DrawPath(path, paint);

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

static sk_sp<SkData> OpenFixtureAsSkData(const char* fixture_name) {
  auto mapping = flutter::testing::OpenFixtureAsMapping(fixture_name);
  if (!mapping) {
    return nullptr;
  }
  auto data = SkData::MakeWithProc(
      mapping->GetMapping(), mapping->GetSize(),
      [](const void* ptr, void* context) {
        delete reinterpret_cast<fml::Mapping*>(context);
      },
      mapping.get());
  mapping.release();
  return data;
}

struct TextRenderOptions {
  Scalar font_size = 50;
  Scalar alpha = 1;
  Point position = Vector2(100, 200);
};

bool RenderTextInCanvas(const std::shared_ptr<Context>& context,
                        Canvas& canvas,
                        const std::string& text,
                        const std::string& font_fixture,
                        TextRenderOptions options = {}) {
  // Draw the baseline.
  canvas.DrawRect({options.position.x - 50, options.position.y, 900, 10},
                  Paint{.color = Color::Aqua().WithAlpha(0.25)});

  // Mark the point at which the text is drawn.
  canvas.DrawCircle(options.position, 5.0,
                    Paint{.color = Color::Red().WithAlpha(0.25)});

  // Construct the text blob.
  auto mapping = OpenFixtureAsSkData(font_fixture.c_str());
  if (!mapping) {
    return false;
  }
  SkFont sk_font(SkTypeface::MakeFromData(mapping), options.font_size);
  auto blob = SkTextBlob::MakeFromString(text.c_str(), sk_font);
  if (!blob) {
    return false;
  }

  // Create the Impeller text frame and draw it at the designated baseline.
  auto frame = TextFrameFromTextBlob(blob);

  Paint text_paint;
  text_paint.color = Color::Yellow().WithAlpha(options.alpha);
  canvas.DrawTextFrame(frame, options.position, text_paint);
  return true;
}

TEST_P(AiksTest, CanRenderTextFrame) {
  Canvas canvas;
  canvas.DrawPaint({.color = Color(0.1, 0.1, 0.1, 1.0)});
  ASSERT_TRUE(RenderTextInCanvas(
      GetContext(), canvas, "the quick brown fox jumped over the lazy dog!.?",
      "Roboto-Regular.ttf"));
  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, TextFrameSubpixelAlignment) {
  std::array<Scalar, 20> phase_offsets;
  for (Scalar& offset : phase_offsets) {
    auto rand = std::rand();  // NOLINT
    offset = (static_cast<float>(rand) / static_cast<float>(RAND_MAX)) * k2Pi;
  }

  auto callback = [&](AiksContext& renderer,
                      RenderTarget& render_target) -> bool {
    static float font_size = 20;
    static float phase_variation = 0.2;
    static float speed = 0.5;
    static float magnitude = 100;
    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SliderFloat("Font size", &font_size, 5, 50);
    ImGui::SliderFloat("Phase variation", &phase_variation, 0, 1);
    ImGui::SliderFloat("Oscillation speed", &speed, 0, 2);
    ImGui::SliderFloat("Oscillation magnitude", &magnitude, 0, 300);
    ImGui::End();

    Canvas canvas;
    canvas.Scale(GetContentScale());

    for (size_t i = 0; i < phase_offsets.size(); i++) {
      auto position = Point(
          200 + magnitude * std::sin((-phase_offsets[i] * phase_variation +
                                      GetSecondsElapsed() * speed)),  //
          200 + i * font_size * 1.1                                   //
      );
      if (!RenderTextInCanvas(GetContext(), canvas,
                              "the quick brown fox jumped over "
                              "the lazy dog!.?",
                              "Roboto-Regular.ttf",
                              {.font_size = font_size, .position = position})) {
        return false;
      }
    }
    return renderer.Render(canvas.EndRecordingAsPicture(), render_target);
  };

  ASSERT_TRUE(OpenPlaygroundHere(callback));
}

TEST_P(AiksTest, CanRenderItalicizedText) {
  Canvas canvas;
  canvas.DrawPaint({.color = Color(0.1, 0.1, 0.1, 1.0)});

  ASSERT_TRUE(RenderTextInCanvas(
      GetContext(), canvas, "the quick brown fox jumped over the lazy dog!.?",
      "HomemadeApple.ttf"));
  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanRenderEmojiTextFrame) {
  Canvas canvas;
  canvas.DrawPaint({.color = Color(0.1, 0.1, 0.1, 1.0)});

  ASSERT_TRUE(RenderTextInCanvas(GetContext(), canvas,
                                 "😀 😃 😄 😁 😆 😅 😂 🤣 🥲 😊",
#if FML_OS_MACOSX
                                 "Apple Color Emoji.ttc"));
#else
                                 "NotoColorEmoji.ttf"));
#endif
  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanRenderEmojiTextFrameWithAlpha) {
  Canvas canvas;
  canvas.DrawPaint({.color = Color(0.1, 0.1, 0.1, 1.0)});

  ASSERT_TRUE(RenderTextInCanvas(GetContext(), canvas,
                                 "😀 😃 😄 😁 😆 😅 😂 🤣 🥲 😊",
#if FML_OS_MACOSX
                                 "Apple Color Emoji.ttc", { .alpha = 0.5 }
#else
                                 "NotoColorEmoji.ttf", {.alpha = 0.5}
#endif
                                 ));
  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanRenderTextInSaveLayer) {
  Canvas canvas;
  canvas.DrawPaint({.color = Color(0.1, 0.1, 0.1, 1.0)});

  canvas.Translate({100, 100});
  canvas.Scale(Vector2{0.5, 0.5});

  // Blend the layer with the parent pass using kClear to expose the coverage.
  canvas.SaveLayer({.blend_mode = BlendMode::kClear});
  ASSERT_TRUE(RenderTextInCanvas(
      GetContext(), canvas, "the quick brown fox jumped over the lazy dog!.?",
      "Roboto-Regular.ttf"));
  canvas.Restore();

  // Render the text again over the cleared coverage rect.
  ASSERT_TRUE(RenderTextInCanvas(
      GetContext(), canvas, "the quick brown fox jumped over the lazy dog!.?",
      "Roboto-Regular.ttf"));

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanRenderTextOutsideBoundaries) {
  Canvas canvas;
  canvas.Translate({200, 150});

  // Construct the text blob.
  auto mapping = OpenFixtureAsSkData("wtf.otf");
  ASSERT_NE(mapping, nullptr);

  Scalar font_size = 80;
  SkFont sk_font(SkTypeface::MakeFromData(mapping), font_size);

  Paint text_paint;
  text_paint.color = Color::Blue().WithAlpha(0.8);

  struct {
    Point position;
    const char* text;
  } text[] = {{Point(0, 0), "0F0F0F0"},
              {Point(1, 2), "789"},
              {Point(1, 3), "456"},
              {Point(1, 4), "123"},
              {Point(0, 6), "0F0F0F0"}};
  for (auto& t : text) {
    canvas.Save();
    canvas.Translate(t.position * Point(font_size * 2, font_size * 1.1));
    {
      auto blob = SkTextBlob::MakeFromString(t.text, sk_font);
      ASSERT_NE(blob, nullptr);
      auto frame = TextFrameFromTextBlob(blob);
      canvas.DrawTextFrame(frame, Point(), text_paint);
    }
    canvas.Restore();
  }

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, TextRotated) {
  Canvas canvas;
  canvas.Scale(GetContentScale());
  canvas.DrawPaint({.color = Color(0.1, 0.1, 0.1, 1.0)});

  canvas.Transform(Matrix(0.25, -0.3, 0, -0.002,  //
                          0, 0.5, 0, 0,           //
                          0, 0, 0.3, 0,           //
                          100, 100, 0, 1.3));
  ASSERT_TRUE(RenderTextInCanvas(
      GetContext(), canvas, "the quick brown fox jumped over the lazy dog!.?",
      "Roboto-Regular.ttf"));

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanDrawPaint) {
  Paint paint;
  paint.color = Color::MediumTurquoise();
  Canvas canvas;
  canvas.Scale(Vector2(0.2, 0.2));
  canvas.DrawPaint(paint);
  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, PaintBlendModeIsRespected) {
  Paint paint;
  Canvas canvas;
  // Default is kSourceOver.
  paint.color = Color(1, 0, 0, 0.5);
  canvas.DrawCircle(Point(150, 200), 100, paint);
  paint.color = Color(0, 1, 0, 0.5);
  canvas.DrawCircle(Point(250, 200), 100, paint);

  paint.blend_mode = BlendMode::kPlus;
  paint.color = Color::Red();
  canvas.DrawCircle(Point(450, 250), 100, paint);
  paint.color = Color::Green();
  canvas.DrawCircle(Point(550, 250), 100, paint);
  paint.color = Color::Blue();
  canvas.DrawCircle(Point(500, 150), 100, paint);
  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

#define BLEND_MODE_TUPLE(blend_mode) {#blend_mode, BlendMode::k##blend_mode},

TEST_P(AiksTest, ColorWheel) {
  // Compare with https://fiddle.skia.org/c/@BlendModes

  std::vector<const char*> blend_mode_names;
  std::vector<BlendMode> blend_mode_values;
  {
    const std::vector<std::tuple<const char*, BlendMode>> blends = {
        IMPELLER_FOR_EACH_BLEND_MODE(BLEND_MODE_TUPLE)};
    assert(blends.size() ==
           static_cast<size_t>(Entity::kLastAdvancedBlendMode) + 1);
    for (const auto& [name, mode] : blends) {
      blend_mode_names.push_back(name);
      blend_mode_values.push_back(mode);
    }
  }

  auto draw_color_wheel = [](Canvas& canvas) {
    /// color_wheel_sampler: r=0 -> fuchsia, r=2pi/3 -> yellow, r=4pi/3 ->
    /// cyan domain: r >= 0 (because modulo used is non euclidean)
    auto color_wheel_sampler = [](Radians r) {
      Scalar x = r.radians / k2Pi + 1;

      // https://www.desmos.com/calculator/6nhjelyoaj
      auto color_cycle = [](Scalar x) {
        Scalar cycle = std::fmod(x, 6.0f);
        return std::max(0.0f, std::min(1.0f, 2 - std::abs(2 - cycle)));
      };
      return Color(color_cycle(6 * x + 1),  //
                   color_cycle(6 * x - 1),  //
                   color_cycle(6 * x - 3),  //
                   1);
    };

    Paint paint;
    paint.blend_mode = BlendMode::kSourceOver;

    // Draw a fancy color wheel for the backdrop.
    // https://www.desmos.com/calculator/xw7kafthwd
    const int max_dist = 900;
    for (int i = 0; i <= 900; i++) {
      Radians r(kPhi / k2Pi * i);
      Scalar distance = r.radians / std::powf(4.12, 0.0026 * r.radians);
      Scalar normalized_distance = static_cast<Scalar>(i) / max_dist;

      paint.color =
          color_wheel_sampler(r).WithAlpha(1.0f - normalized_distance);
      Point position(distance * std::sin(r.radians),
                     -distance * std::cos(r.radians));

      canvas.DrawCircle(position, 9 + normalized_distance * 3, paint);
    }
  };

  std::shared_ptr<Image> color_wheel_image;
  Matrix color_wheel_transform;

  auto callback = [&](AiksContext& renderer, RenderTarget& render_target) {
    // UI state.
    static bool cache_the_wheel = true;
    static int current_blend_index = 3;
    static float dst_alpha = 1;
    static float src_alpha = 1;
    static Color color0 = Color::Red();
    static Color color1 = Color::Green();
    static Color color2 = Color::Blue();

    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    {
      ImGui::Checkbox("Cache the wheel", &cache_the_wheel);
      ImGui::ListBox("Blending mode", &current_blend_index,
                     blend_mode_names.data(), blend_mode_names.size());
      ImGui::SliderFloat("Source alpha", &src_alpha, 0, 1);
      ImGui::ColorEdit4("Color A", reinterpret_cast<float*>(&color0));
      ImGui::ColorEdit4("Color B", reinterpret_cast<float*>(&color1));
      ImGui::ColorEdit4("Color C", reinterpret_cast<float*>(&color2));
      ImGui::SliderFloat("Destination alpha", &dst_alpha, 0, 1);
    }
    ImGui::End();

    static Point content_scale;
    Point new_content_scale = GetContentScale();

    if (!cache_the_wheel || new_content_scale != content_scale) {
      content_scale = new_content_scale;

      // Render the color wheel to an image.

      Canvas canvas;
      canvas.Scale(content_scale);

      canvas.Translate(Vector2(500, 400));
      canvas.Scale(Vector2(3, 3));

      draw_color_wheel(canvas);
      auto color_wheel_picture = canvas.EndRecordingAsPicture();
      auto snapshot = color_wheel_picture.Snapshot(renderer);
      if (!snapshot.has_value() || !snapshot->texture) {
        return false;
      }
      color_wheel_image = std::make_shared<Image>(snapshot->texture);
      color_wheel_transform = snapshot->transform;
    }

    Canvas canvas;

    // Blit the color wheel backdrop to the screen with managed alpha.
    canvas.SaveLayer({.color = Color::White().WithAlpha(dst_alpha),
                      .blend_mode = BlendMode::kSource});
    {
      canvas.DrawPaint({.color = Color::White()});

      canvas.Save();
      canvas.Transform(color_wheel_transform);
      canvas.DrawImage(color_wheel_image, Point(), Paint());
      canvas.Restore();
    }
    canvas.Restore();

    canvas.Scale(content_scale);
    canvas.Translate(Vector2(500, 400));
    canvas.Scale(Vector2(3, 3));

    // Draw 3 circles to a subpass and blend it in.
    canvas.SaveLayer({.color = Color::White().WithAlpha(src_alpha),
                      .blend_mode = blend_mode_values[current_blend_index]});
    {
      Paint paint;
      paint.blend_mode = BlendMode::kPlus;
      const Scalar x = std::sin(k2Pi / 3);
      const Scalar y = -std::cos(k2Pi / 3);
      paint.color = color0;
      canvas.DrawCircle(Point(-x, y) * 45, 65, paint);
      paint.color = color1;
      canvas.DrawCircle(Point(0, -1) * 45, 65, paint);
      paint.color = color2;
      canvas.DrawCircle(Point(x, y) * 45, 65, paint);
    }
    canvas.Restore();

    return renderer.Render(canvas.EndRecordingAsPicture(), render_target);
  };

  ASSERT_TRUE(OpenPlaygroundHere(callback));
}

TEST_P(AiksTest, TransformMultipliesCorrectly) {
  Canvas canvas;
  ASSERT_MATRIX_NEAR(canvas.GetCurrentTransformation(), Matrix());

  // clang-format off
  canvas.Translate(Vector3(100, 200));
  ASSERT_MATRIX_NEAR(
    canvas.GetCurrentTransformation(),
    Matrix(  1,   0,   0,   0,
             0,   1,   0,   0,
             0,   0,   1,   0,
           100, 200,   0,   1));

  canvas.Rotate(Radians(kPiOver2));
  ASSERT_MATRIX_NEAR(
    canvas.GetCurrentTransformation(),
    Matrix(  0,   1,   0,   0,
            -1,   0,   0,   0,
             0,   0,   1,   0,
           100, 200,   0,   1));

  canvas.Scale(Vector3(2, 3));
  ASSERT_MATRIX_NEAR(
    canvas.GetCurrentTransformation(),
    Matrix(  0,   2,   0,   0,
            -3,   0,   0,   0,
             0,   0,   0,   0,
           100, 200,   0,   1));

  canvas.Translate(Vector3(100, 200));
  ASSERT_MATRIX_NEAR(
    canvas.GetCurrentTransformation(),
    Matrix(   0,   2,   0,   0,
             -3,   0,   0,   0,
              0,   0,   0,   0,
           -500, 400,   0,   1));
  // clang-format on
}

TEST_P(AiksTest, SolidStrokesRenderCorrectly) {
  // Compare with https://fiddle.skia.org/c/027392122bec8ac2b5d5de00a4b9bbe2
  auto callback = [&](AiksContext& renderer, RenderTarget& render_target) {
    static Color color = Color::Black().WithAlpha(0.5);
    static float scale = 3;
    static bool add_circle_clip = true;

    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::ColorEdit4("Color", reinterpret_cast<float*>(&color));
    ImGui::SliderFloat("Scale", &scale, 0, 6);
    ImGui::Checkbox("Circle clip", &add_circle_clip);
    ImGui::End();

    Canvas canvas;
    canvas.Scale(GetContentScale());
    Paint paint;

    paint.color = Color::White();
    canvas.DrawPaint(paint);

    paint.color = color;
    paint.style = Paint::Style::kStroke;
    paint.stroke_width = 10;

    Path path = PathBuilder{}
                    .MoveTo({20, 20})
                    .QuadraticCurveTo({60, 20}, {60, 60})
                    .Close()
                    .MoveTo({60, 20})
                    .QuadraticCurveTo({60, 60}, {20, 60})
                    .TakePath();

    canvas.Scale(Vector2(scale, scale));

    if (add_circle_clip) {
      auto [handle_a, handle_b] = IMPELLER_PLAYGROUND_LINE(
          Point(60, 300), Point(600, 300), 20, Color::Red(), Color::Red());

      auto screen_to_canvas = canvas.GetCurrentTransformation().Invert();
      Point point_a = screen_to_canvas * handle_a * GetContentScale();
      Point point_b = screen_to_canvas * handle_b * GetContentScale();

      Point middle = (point_a + point_b) / 2;
      auto radius = point_a.GetDistance(middle);
      canvas.ClipPath(PathBuilder{}.AddCircle(middle, radius).TakePath());
    }

    for (auto join : {Join::kBevel, Join::kRound, Join::kMiter}) {
      paint.stroke_join = join;
      for (auto cap : {Cap::kButt, Cap::kSquare, Cap::kRound}) {
        paint.stroke_cap = cap;
        canvas.DrawPath(path, paint);
        canvas.Translate({80, 0});
      }
      canvas.Translate({-240, 60});
    }

    return renderer.Render(canvas.EndRecordingAsPicture(), render_target);
  };

  ASSERT_TRUE(OpenPlaygroundHere(callback));
}

TEST_P(AiksTest, GradientStrokesRenderCorrectly) {
  // Compare with https://fiddle.skia.org/c/027392122bec8ac2b5d5de00a4b9bbe2
  auto callback = [&](AiksContext& renderer, RenderTarget& render_target) {
    static float scale = 3;
    static bool add_circle_clip = true;
    const char* tile_mode_names[] = {"Clamp", "Repeat", "Mirror", "Decal"};
    const Entity::TileMode tile_modes[] = {
        Entity::TileMode::kClamp, Entity::TileMode::kRepeat,
        Entity::TileMode::kMirror, Entity::TileMode::kDecal};
    static int selected_tile_mode = 0;
    static float alpha = 1;

    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SliderFloat("Scale", &scale, 0, 6);
    ImGui::Checkbox("Circle clip", &add_circle_clip);
    ImGui::SliderFloat("Alpha", &alpha, 0, 1);
    ImGui::Combo("Tile mode", &selected_tile_mode, tile_mode_names,
                 sizeof(tile_mode_names) / sizeof(char*));
    ImGui::End();

    Canvas canvas;
    canvas.Scale(GetContentScale());
    Paint paint;
    paint.color = Color::White();
    canvas.DrawPaint(paint);

    paint.style = Paint::Style::kStroke;
    paint.color = Color(1.0, 1.0, 1.0, alpha);
    paint.stroke_width = 10;
    auto tile_mode = tile_modes[selected_tile_mode];
    paint.color_source = [tile_mode]() {
      std::vector<Color> colors = {Color{0.9568, 0.2627, 0.2118, 1.0},
                                   Color{0.1294, 0.5882, 0.9529, 1.0}};
      std::vector<Scalar> stops = {0.0, 1.0};
      Matrix matrix = {
          1, 0, 0, 0,  //
          0, 1, 0, 0,  //
          0, 0, 1, 0,  //
          0, 0, 0, 1   //
      };
      auto contents = std::make_shared<LinearGradientContents>();
      contents->SetEndPoints({0, 0}, {50, 50});
      contents->SetColors(std::move(colors));
      contents->SetStops(std::move(stops));
      contents->SetTileMode(tile_mode);
      contents->SetEffectTransform(matrix);
      return contents;
    };

    Path path = PathBuilder{}
                    .MoveTo({20, 20})
                    .QuadraticCurveTo({60, 20}, {60, 60})
                    .Close()
                    .MoveTo({60, 20})
                    .QuadraticCurveTo({60, 60}, {20, 60})
                    .TakePath();

    canvas.Scale(Vector2(scale, scale));

    if (add_circle_clip) {
      auto [handle_a, handle_b] = IMPELLER_PLAYGROUND_LINE(
          Point(60, 300), Point(600, 300), 20, Color::Red(), Color::Red());

      auto screen_to_canvas = canvas.GetCurrentTransformation().Invert();
      Point point_a = screen_to_canvas * handle_a * GetContentScale();
      Point point_b = screen_to_canvas * handle_b * GetContentScale();

      Point middle = (point_a + point_b) / 2;
      auto radius = point_a.GetDistance(middle);
      canvas.ClipPath(PathBuilder{}.AddCircle(middle, radius).TakePath());
    }

    for (auto join : {Join::kBevel, Join::kRound, Join::kMiter}) {
      paint.stroke_join = join;
      for (auto cap : {Cap::kButt, Cap::kSquare, Cap::kRound}) {
        paint.stroke_cap = cap;
        canvas.DrawPath(path, paint);
        canvas.Translate({80, 0});
      }
      canvas.Translate({-240, 60});
    }

    return renderer.Render(canvas.EndRecordingAsPicture(), render_target);
  };

  ASSERT_TRUE(OpenPlaygroundHere(callback));
}

TEST_P(AiksTest, CoverageOriginShouldBeAccountedForInSubpasses) {
  auto callback = [&](AiksContext& renderer, RenderTarget& render_target) {
    Canvas canvas;
    canvas.Scale(GetContentScale());

    Paint alpha;
    alpha.color = Color::Red().WithAlpha(0.5);

    auto current = Point{25, 25};
    const auto offset = Point{25, 25};
    const auto size = Size(100, 100);

    auto [b0, b1] = IMPELLER_PLAYGROUND_LINE(Point(40, 40), Point(160, 160), 10,
                                             Color::White(), Color::White());
    auto bounds = Rect::MakeLTRB(b0.x, b0.y, b1.x, b1.y);

    canvas.DrawRect(bounds, Paint{.color = Color::Yellow(),
                                  .stroke_width = 5.0f,
                                  .style = Paint::Style::kStroke});

    canvas.SaveLayer(alpha, bounds);

    canvas.DrawRect({current, size}, Paint{.color = Color::Red()});
    canvas.DrawRect({current += offset, size}, Paint{.color = Color::Green()});
    canvas.DrawRect({current += offset, size}, Paint{.color = Color::Blue()});

    canvas.Restore();

    return renderer.Render(canvas.EndRecordingAsPicture(), render_target);
  };

  ASSERT_TRUE(OpenPlaygroundHere(callback));
}

TEST_P(AiksTest, DrawRectStrokesRenderCorrectly) {
  Canvas canvas;
  Paint paint;
  paint.color = Color::Red();
  paint.style = Paint::Style::kStroke;
  paint.stroke_width = 10;

  canvas.Translate({100, 100});
  canvas.DrawPath(
      PathBuilder{}.AddRect(Rect::MakeSize(Size{100, 100})).TakePath(),
      {paint});

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, SaveLayerDrawsBehindSubsequentEntities) {
  // Compare with https://fiddle.skia.org/c/9e03de8567ffb49e7e83f53b64bcf636
  Canvas canvas;
  Paint paint;

  paint.color = Color::Black();
  Rect rect(25, 25, 25, 25);
  canvas.DrawRect(rect, paint);

  canvas.Translate({10, 10});
  canvas.SaveLayer({});

  paint.color = Color::Green();
  canvas.DrawRect(rect, paint);

  canvas.Restore();

  canvas.Translate({10, 10});
  paint.color = Color::Red();
  canvas.DrawRect(rect, paint);

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, SiblingSaveLayerBoundsAreRespected) {
  Canvas canvas;
  Paint paint;
  Rect rect(0, 0, 1000, 1000);

  // Black, green, and red squares offset by [10, 10].
  {
    canvas.SaveLayer({}, Rect::MakeXYWH(25, 25, 25, 25));
    paint.color = Color::Black();
    canvas.DrawRect(rect, paint);
    canvas.Restore();
  }

  {
    canvas.SaveLayer({}, Rect::MakeXYWH(35, 35, 25, 25));
    paint.color = Color::Green();
    canvas.DrawRect(rect, paint);
    canvas.Restore();
  }

  {
    canvas.SaveLayer({}, Rect::MakeXYWH(45, 45, 25, 25));
    paint.color = Color::Red();
    canvas.DrawRect(rect, paint);
    canvas.Restore();
  }

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanRenderClippedLayers) {
  Canvas canvas;

  canvas.DrawPaint({.color = Color::White()});

  // Draw a green circle on the screen.
  {
    // Increase the clip depth for the savelayer to contend with.
    canvas.ClipPath(PathBuilder{}.AddCircle({100, 100}, 50).TakePath());

    canvas.SaveLayer({}, Rect::MakeXYWH(50, 50, 100, 100));

    // Fill the layer with white.
    canvas.DrawRect(Rect::MakeSize(Size{400, 400}), {.color = Color::White()});
    // Fill the layer with green, but do so with a color blend that can't be
    // collapsed into the parent pass.
    canvas.DrawRect(
        Rect::MakeSize(Size{400, 400}),
        {.color = Color::Green(), .blend_mode = BlendMode::kColorBurn});
  }

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, SaveLayerFiltersScaleWithTransform) {
  Canvas canvas;
  canvas.Scale(GetContentScale());
  canvas.Translate(Vector2(100, 100));

  auto texture = std::make_shared<Image>(CreateTextureForFixture("boston.jpg"));
  auto draw_image_layer = [&canvas, &texture](const Paint& paint) {
    canvas.SaveLayer(paint);
    canvas.DrawImage(texture, {}, Paint{});
    canvas.Restore();
  };

  Paint effect_paint;
  effect_paint.mask_blur_descriptor = Paint::MaskBlurDescriptor{
      .style = FilterContents::BlurStyle::kNormal,
      .sigma = Sigma{6},
  };
  draw_image_layer(effect_paint);

  canvas.Translate(Vector2(300, 300));
  canvas.Scale(Vector2(3, 3));
  draw_image_layer(effect_paint);

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, SceneColorSource) {
  // Load up the scene.
  auto mapping =
      flutter::testing::OpenFixtureAsMapping("flutter_logo_baked.glb.ipscene");
  ASSERT_NE(mapping, nullptr);

  std::shared_ptr<scene::Node> gltf_scene = scene::Node::MakeFromFlatbuffer(
      *mapping, *GetContext()->GetResourceAllocator());
  ASSERT_NE(gltf_scene, nullptr);

  auto callback = [&](AiksContext& renderer, RenderTarget& render_target) {
    Paint paint;

    ImGui::Begin("Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    static Scalar distance = 2;
    ImGui::SliderFloat("Distance", &distance, 0, 4);
    static Scalar y_pos = 0;
    ImGui::SliderFloat("Y", &y_pos, -3, 3);
    static Scalar fov = 45;
    ImGui::SliderFloat("FOV", &fov, 1, 180);
    ImGui::End();

    paint.color_source_type = Paint::ColorSourceType::kScene;
    paint.color_source = [&]() {
      Scalar angle = GetSecondsElapsed();
      auto camera_position = Vector3(distance * std::sin(angle), y_pos,
                                     -distance * std::cos(angle));
      auto contents = std::make_shared<SceneContents>();
      contents->SetNode(gltf_scene);
      contents->SetCameraTransform(
          Matrix::MakePerspective(Degrees(fov), GetWindowSize(), 0.1, 1000) *
          Matrix::MakeLookAt(camera_position, {0, 0, 0}, {0, 1, 0}));
      return contents;
    };

    Canvas canvas;
    canvas.DrawPaint(Paint{.color = Color::MakeRGBA8(0xf9, 0xf9, 0xf9, 0xff)});
    canvas.Scale(GetContentScale());
    canvas.DrawPaint(paint);
    return renderer.Render(canvas.EndRecordingAsPicture(), render_target);
  };

  ASSERT_TRUE(OpenPlaygroundHere(callback));
}

TEST_P(AiksTest, PaintWithFilters) {
  // validate that a paint with a color filter "HasFilters", no other filters
  // impact this setting.
  Paint paint;

  ASSERT_FALSE(paint.HasColorFilter());

  paint.color_filter = [](FilterInput::Ref input) {
    return ColorFilterContents::MakeBlend(BlendMode::kSourceOver,
                                          {std::move(input)}, Color::Blue());
  };

  ASSERT_TRUE(paint.HasColorFilter());

  paint.image_filter = [](const FilterInput::Ref& input,
                          const Matrix& effect_transform, bool is_subpass) {
    return FilterContents::MakeGaussianBlur(
        input, Sigma(1.0), Sigma(1.0), FilterContents::BlurStyle::kNormal,
        Entity::TileMode::kClamp, effect_transform);
  };

  ASSERT_TRUE(paint.HasColorFilter());

  paint.mask_blur_descriptor = {};

  ASSERT_TRUE(paint.HasColorFilter());

  paint.color_filter = std::nullopt;

  ASSERT_FALSE(paint.HasColorFilter());
}

TEST_P(AiksTest, OpacityPeepHoleApplicationTest) {
  auto entity_pass = std::make_shared<EntityPass>();
  auto rect = Rect::MakeLTRB(0, 0, 100, 100);
  Paint paint;
  paint.color = Color::White().WithAlpha(0.5);
  paint.color_filter = [](FilterInput::Ref input) {
    return ColorFilterContents::MakeBlend(BlendMode::kSourceOver,
                                          {std::move(input)}, Color::Blue());
  };

  // Paint has color filter, can't elide.
  auto delegate = std::make_shared<OpacityPeepholePassDelegate>(paint, rect);
  ASSERT_FALSE(delegate->CanCollapseIntoParentPass(entity_pass.get()));

  paint.color_filter = std::nullopt;
  paint.image_filter = [](const FilterInput::Ref& input,
                          const Matrix& effect_transform, bool is_subpass) {
    return FilterContents::MakeGaussianBlur(
        input, Sigma(1.0), Sigma(1.0), FilterContents::BlurStyle::kNormal,
        Entity::TileMode::kClamp, effect_transform);
  };

  // Paint has image filter, can't elide.
  delegate = std::make_shared<OpacityPeepholePassDelegate>(paint, rect);
  ASSERT_FALSE(delegate->CanCollapseIntoParentPass(entity_pass.get()));

  paint.image_filter = std::nullopt;
  paint.color = Color::Red();

  // Paint has no alpha, can't elide;
  delegate = std::make_shared<OpacityPeepholePassDelegate>(paint, rect);
  ASSERT_FALSE(delegate->CanCollapseIntoParentPass(entity_pass.get()));

  // Positive test.
  Entity entity;
  entity.SetContents(SolidColorContents::Make(
      PathBuilder{}.AddRect(rect).TakePath(), Color::Red()));
  entity_pass->AddEntity(entity);
  paint.color = Color::Red().WithAlpha(0.5);

  delegate = std::make_shared<OpacityPeepholePassDelegate>(paint, rect);
  ASSERT_TRUE(delegate->CanCollapseIntoParentPass(entity_pass.get()));
}

TEST_P(AiksTest, DrawPaintAbsorbsClears) {
  Canvas canvas;
  canvas.DrawPaint({.color = Color::Red(), .blend_mode = BlendMode::kSource});
  canvas.DrawPaint(
      {.color = Color::CornflowerBlue(), .blend_mode = BlendMode::kSource});

  Picture picture = canvas.EndRecordingAsPicture();

  ASSERT_EQ(picture.pass->GetElementCount(), 0u);
  ASSERT_EQ(picture.pass->GetClearColor(), Color::CornflowerBlue());
}

static Picture BlendModeSaveLayerTest(BlendMode blend_mode) {
  Canvas canvas;
  canvas.DrawPaint({.color = Color::CornflowerBlue().WithAlpha(0.75)});
  canvas.SaveLayer({.blend_mode = blend_mode});
  for (auto& color : {Color::White(), Color::LimeGreen(), Color::Black()}) {
    canvas.DrawRect({100, 100, 200, 200}, {.color = color.WithAlpha(0.75)});
    canvas.Translate(Vector2(150, 100));
  }
  return canvas.EndRecordingAsPicture();
}

#define BLEND_MODE_TEST(blend_mode)                                       \
  TEST_P(AiksTest, BlendModeSaveLayer##blend_mode) {                      \
    OpenPlaygroundHere(BlendModeSaveLayerTest(BlendMode::k##blend_mode)); \
  }
IMPELLER_FOR_EACH_BLEND_MODE(BLEND_MODE_TEST)

TEST_P(AiksTest, TranslucentSaveLayerWithAdvancedBlendModeDrawsCorrectly) {
  Canvas canvas;
  canvas.DrawRect({0, 0, 400, 400}, {.color = Color::Red()});
  canvas.SaveLayer({
      .color = Color::Black().WithAlpha(0.5),
      .blend_mode = BlendMode::kLighten,
  });
  canvas.DrawCircle({200, 200}, 100, {.color = Color::Green()});
  canvas.Restore();
  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

/// This is a regression check for https://github.com/flutter/engine/pull/41129
/// The entire screen is green if successful. If failing, no frames will render,
/// or the entire screen will be transparent black.
TEST_P(AiksTest, CanRenderTinyOverlappingSubpasses) {
  Canvas canvas;
  canvas.DrawPaint({.color = Color::Red()});

  // Draw two overlapping subpixel circles.
  canvas.SaveLayer({});
  canvas.DrawCircle({100, 100}, 0.1, {.color = Color::Yellow()});
  canvas.Restore();
  canvas.SaveLayer({});
  canvas.DrawCircle({100, 100}, 0.1, {.color = Color::Yellow()});
  canvas.Restore();

  canvas.DrawPaint({.color = Color::Green()});

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

TEST_P(AiksTest, CanRenderBackdropBlurHugeSigma) {
  Canvas canvas;
  canvas.DrawCircle({400, 400}, 300, {.color = Color::Green()});
  canvas.SaveLayer({.blend_mode = BlendMode::kSource}, std::nullopt,
                   [](const FilterInput::Ref& input,
                      const Matrix& effect_transform, bool is_subpass) {
                     return FilterContents::MakeGaussianBlur(
                         input, Sigma(999999), Sigma(999999),
                         FilterContents::BlurStyle::kNormal,
                         Entity::TileMode::kClamp, effect_transform);
                   });
  canvas.Restore();

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

// Regression test for https://github.com/flutter/flutter/issues/126701 .
TEST_P(AiksTest, CanRenderClippedRuntimeEffects) {
  if (GetParam() != PlaygroundBackend::kMetal) {
    GTEST_SKIP_("This backend doesn't support runtime effects.");
  }

  auto runtime_stage =
      OpenAssetAsRuntimeStage("runtime_stage_example.frag.iplr");
  ASSERT_TRUE(runtime_stage->IsDirty());

  struct FragUniforms {
    Vector2 iResolution;
    Scalar iTime;
  } frag_uniforms = {.iResolution = Vector2(400, 400), .iTime = 100.0};
  auto uniform_data = std::make_shared<std::vector<uint8_t>>();
  uniform_data->resize(sizeof(FragUniforms));
  memcpy(uniform_data->data(), &frag_uniforms, sizeof(FragUniforms));

  std::vector<RuntimeEffectContents::TextureInput> texture_inputs;

  Paint paint;
  paint.color_source = [runtime_stage, uniform_data, texture_inputs]() {
    auto contents = std::make_shared<RuntimeEffectContents>();
    contents->SetRuntimeStage(runtime_stage);
    contents->SetUniformData(uniform_data);
    contents->SetTextureInputs(texture_inputs);
    return contents;
  };

  Canvas canvas;
  canvas.Save();
  canvas.ClipRRect(Rect{0, 0, 400, 400}, 10.0,
                   Entity::ClipOperation::kIntersect);
  canvas.DrawRect(Rect{0, 0, 400, 400}, paint);
  canvas.Restore();

  ASSERT_TRUE(OpenPlaygroundHere(canvas.EndRecordingAsPicture()));
}

}  // namespace testing
}  // namespace impeller
