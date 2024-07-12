// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <impeller/texture.glsl>
#include <impeller/types.glsl>

uniform FrameInfo {
  mat4 mvp;
  float texture_sampler_y_coord_scale;
  float alpha_mask_sampler_y_coord_scale;
}
frame_info;

in vec2 vertices;
in vec2 texture_coords;
in vec2 src_texture_coords;

out vec2 v_texture_coords;
out vec2 v_src_texture_coords;

void main() {
  gl_Position = frame_info.mvp * vec4(vertices, 0.0, 1.0);
  v_texture_coords =
      IPRemapCoords(texture_coords, frame_info.texture_sampler_y_coord_scale);
  v_src_texture_coords = IPRemapCoords(
      src_texture_coords, frame_info.alpha_mask_sampler_y_coord_scale);
}
