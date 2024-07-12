// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/capabilities.h"

namespace impeller {

Capabilities::Capabilities() = default;

Capabilities::~Capabilities() = default;

class StandardCapabilities final : public Capabilities {
 public:
  // |Capabilities|
  ~StandardCapabilities() override = default;

  // |Capabilities|
  bool HasThreadingRestrictions() const override {
    return has_threading_restrictions_;
  }

  // |Capabilities|
  bool SupportsOffscreenMSAA() const override {
    return supports_offscreen_msaa_;
  }

  // |Capabilities|
  bool SupportsSSBO() const override { return supports_ssbo_; }

  // |Capabilities|
  bool SupportsTextureToTextureBlits() const override {
    return supports_texture_to_texture_blits_;
  }

  // |Capabilities|
  bool SupportsFramebufferFetch() const override {
    return supports_framebuffer_fetch_;
  }

  // |Capabilities|
  bool SupportsCompute() const override { return supports_compute_; }

  // |Capabilities|
  bool SupportsComputeSubgroups() const override {
    return supports_compute_subgroups_;
  }

  // |Capabilities|
  bool SupportsReadFromResolve() const override {
    return supports_read_from_resolve_;
  }

  // |Capabilities|
  bool SupportsDecalTileMode() const override {
    return supports_decal_tile_mode_;
  }

  // |Capabilities|
  PixelFormat GetDefaultColorFormat() const override {
    return default_color_format_;
  }

  // |Capabilities|
  PixelFormat GetDefaultStencilFormat() const override {
    return default_stencil_format_;
  }

 private:
  StandardCapabilities(bool has_threading_restrictions,
                       bool supports_offscreen_msaa,
                       bool supports_ssbo,
                       bool supports_texture_to_texture_blits,
                       bool supports_framebuffer_fetch,
                       bool supports_compute,
                       bool supports_compute_subgroups,
                       bool supports_read_from_resolve,
                       bool supports_decal_tile_mode,
                       PixelFormat default_color_format,
                       PixelFormat default_stencil_format)
      : has_threading_restrictions_(has_threading_restrictions),
        supports_offscreen_msaa_(supports_offscreen_msaa),
        supports_ssbo_(supports_ssbo),
        supports_texture_to_texture_blits_(supports_texture_to_texture_blits),
        supports_framebuffer_fetch_(supports_framebuffer_fetch),
        supports_compute_(supports_compute),
        supports_compute_subgroups_(supports_compute_subgroups),
        supports_read_from_resolve_(supports_read_from_resolve),
        supports_decal_tile_mode_(supports_decal_tile_mode),
        default_color_format_(default_color_format),
        default_stencil_format_(default_stencil_format) {}

  friend class CapabilitiesBuilder;

  bool has_threading_restrictions_ = false;
  bool supports_offscreen_msaa_ = false;
  bool supports_ssbo_ = false;
  bool supports_texture_to_texture_blits_ = false;
  bool supports_framebuffer_fetch_ = false;
  bool supports_compute_ = false;
  bool supports_compute_subgroups_ = false;
  bool supports_read_from_resolve_ = false;
  bool supports_decal_tile_mode_ = false;
  PixelFormat default_color_format_ = PixelFormat::kUnknown;
  PixelFormat default_stencil_format_ = PixelFormat::kUnknown;

  FML_DISALLOW_COPY_AND_ASSIGN(StandardCapabilities);
};

CapabilitiesBuilder::CapabilitiesBuilder() = default;

CapabilitiesBuilder::~CapabilitiesBuilder() = default;

CapabilitiesBuilder& CapabilitiesBuilder::SetHasThreadingRestrictions(
    bool value) {
  has_threading_restrictions_ = value;
  return *this;
}

CapabilitiesBuilder& CapabilitiesBuilder::SetSupportsOffscreenMSAA(bool value) {
  supports_offscreen_msaa_ = value;
  return *this;
}

CapabilitiesBuilder& CapabilitiesBuilder::SetSupportsSSBO(bool value) {
  supports_ssbo_ = value;
  return *this;
}

CapabilitiesBuilder& CapabilitiesBuilder::SetSupportsTextureToTextureBlits(
    bool value) {
  supports_texture_to_texture_blits_ = value;
  return *this;
}

CapabilitiesBuilder& CapabilitiesBuilder::SetSupportsFramebufferFetch(
    bool value) {
  supports_framebuffer_fetch_ = value;
  return *this;
}

CapabilitiesBuilder& CapabilitiesBuilder::SetSupportsCompute(bool compute,
                                                             bool subgroups) {
  supports_compute_ = compute;
  supports_compute_subgroups_ = subgroups;
  return *this;
}

CapabilitiesBuilder& CapabilitiesBuilder::SetSupportsReadFromResolve(
    bool read_from_resolve) {
  supports_read_from_resolve_ = read_from_resolve;
  return *this;
}

CapabilitiesBuilder& CapabilitiesBuilder::SetDefaultColorFormat(
    PixelFormat value) {
  default_color_format_ = value;
  return *this;
}

CapabilitiesBuilder& CapabilitiesBuilder::SetDefaultStencilFormat(
    PixelFormat value) {
  default_stencil_format_ = value;
  return *this;
}

CapabilitiesBuilder& CapabilitiesBuilder::SetSupportsDecalTileMode(bool value) {
  supports_decal_tile_mode_ = value;
  return *this;
}

std::unique_ptr<Capabilities> CapabilitiesBuilder::Build() {
  return std::unique_ptr<StandardCapabilities>(new StandardCapabilities(  //
      has_threading_restrictions_,                                        //
      supports_offscreen_msaa_,                                           //
      supports_ssbo_,                                                     //
      supports_texture_to_texture_blits_,                                 //
      supports_framebuffer_fetch_,                                        //
      supports_compute_,                                                  //
      supports_compute_subgroups_,                                        //
      supports_read_from_resolve_,                                        //
      supports_decal_tile_mode_,                                          //
      *default_color_format_,                                             //
      *default_stencil_format_                                            //
      ));
}

}  // namespace impeller
