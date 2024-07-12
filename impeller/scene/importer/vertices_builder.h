// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <cstddef>
#include <map>

#include "flutter/fml/macros.h"
#include "impeller/geometry/matrix.h"
#include "impeller/scene/importer/scene_flatbuffers.h"

namespace impeller {
namespace scene {
namespace importer {

//------------------------------------------------------------------------------
/// VerticesBuilder
///

class VerticesBuilder {
 public:
  static std::unique_ptr<VerticesBuilder> MakeUnskinned();

  static std::unique_ptr<VerticesBuilder> MakeSkinned();

  enum class ComponentType {
    kSignedByte = 5120,
    kUnsignedByte,
    kSignedShort,
    kUnsignedShort,
    kSignedInt,
    kUnsignedInt,
    kFloat,
  };

  enum class AttributeType {
    kPosition,
    kNormal,
    kTangent,
    kTextureCoords,
    kColor,
    kJoints,
    kWeights,
  };

  using ComponentConverter = std::function<
      Scalar(const void* source, size_t byte_offset, bool normalized)>;
  struct ComponentProperties {
    size_t size_bytes = 0;
    ComponentConverter convert_proc;
  };

  struct AttributeProperties;
  using AttributeWriter =
      std::function<void(Scalar* destination,
                         const void* source,
                         const ComponentProperties& component_props,
                         const AttributeProperties& attribute_props)>;
  struct AttributeProperties {
    size_t offset_bytes = 0;
    size_t size_bytes = 0;
    size_t component_count = 0;
    AttributeWriter write_proc;
  };

  VerticesBuilder();

  virtual ~VerticesBuilder();

  virtual void WriteFBVertices(fb::MeshPrimitiveT& primitive) const = 0;

  virtual void SetAttributeFromBuffer(AttributeType attribute,
                                      ComponentType component_type,
                                      const void* buffer_start,
                                      size_t attribute_stride_bytes,
                                      size_t attribute_count) = 0;

 protected:
  static void WriteAttribute(void* destination,
                             size_t destination_stride_bytes,
                             AttributeType attribute,
                             ComponentType component_type,
                             const void* source,
                             size_t attribute_stride_bytes,
                             size_t attribute_count);

 private:
  static std::map<VerticesBuilder::AttributeType,
                  VerticesBuilder::AttributeProperties>
      kAttributeTypes;

  FML_DISALLOW_COPY_AND_ASSIGN(VerticesBuilder);
};

//------------------------------------------------------------------------------
/// UnskinnedVerticesBuilder
///

class UnskinnedVerticesBuilder final : public VerticesBuilder {
 public:
  struct Vertex {
    Vector3 position;
    Vector3 normal;
    Vector4 tangent;
    Vector2 texture_coords;
    Color color = Color::White();
  };

  UnskinnedVerticesBuilder();

  virtual ~UnskinnedVerticesBuilder() override;

  // |VerticesBuilder|
  void WriteFBVertices(fb::MeshPrimitiveT& primitive) const override;

  // |VerticesBuilder|
  void SetAttributeFromBuffer(AttributeType attribute,
                              ComponentType component_type,
                              const void* buffer_start,
                              size_t attribute_stride_bytes,
                              size_t attribute_count) override;

 private:
  std::vector<Vertex> vertices_;

  FML_DISALLOW_COPY_AND_ASSIGN(UnskinnedVerticesBuilder);
};

//------------------------------------------------------------------------------
/// SkinnedVerticesBuilder
///

class SkinnedVerticesBuilder final : public VerticesBuilder {
 public:
  struct Vertex {
    UnskinnedVerticesBuilder::Vertex vertex;
    Vector4 joints;
    Vector4 weights;
  };

  SkinnedVerticesBuilder();

  virtual ~SkinnedVerticesBuilder() override;

  // |VerticesBuilder|
  void WriteFBVertices(fb::MeshPrimitiveT& primitive) const override;

  // |VerticesBuilder|
  void SetAttributeFromBuffer(AttributeType attribute,
                              ComponentType component_type,
                              const void* buffer_start,
                              size_t attribute_stride_bytes,
                              size_t attribute_count) override;

 private:
  std::vector<Vertex> vertices_;

  FML_DISALLOW_COPY_AND_ASSIGN(SkinnedVerticesBuilder);
};

}  // namespace importer
}  // namespace scene
}  // namespace impeller
