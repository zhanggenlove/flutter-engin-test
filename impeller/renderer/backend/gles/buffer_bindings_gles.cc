// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/renderer/backend/gles/buffer_bindings_gles.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "impeller/base/config.h"
#include "impeller/base/validation.h"
#include "impeller/renderer/backend/gles/device_buffer_gles.h"
#include "impeller/renderer/backend/gles/formats_gles.h"
#include "impeller/renderer/backend/gles/sampler_gles.h"
#include "impeller/renderer/backend/gles/texture_gles.h"

namespace impeller {

BufferBindingsGLES::BufferBindingsGLES() = default;

BufferBindingsGLES::~BufferBindingsGLES() = default;

bool BufferBindingsGLES::RegisterVertexStageInput(
    const ProcTableGLES& gl,
    const std::vector<ShaderStageIOSlot>& p_inputs) {
  // Attrib locations have to be iterated over in order of location because we
  // will be calculating offsets later.
  auto inputs = p_inputs;
  std::sort(inputs.begin(), inputs.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.location < rhs.location;
  });

  std::vector<VertexAttribPointer> vertex_attrib_arrays;
  size_t offset = 0u;
  for (const auto& input : inputs) {
    VertexAttribPointer attrib;
    attrib.index = input.location;
    // Component counts must be 1, 2, 3 or 4. Do that validation now.
    if (input.vec_size < 1u || input.vec_size > 4u) {
      return false;
    }
    attrib.size = input.vec_size;
    auto type = ToVertexAttribType(input.type);
    if (!type.has_value()) {
      return false;
    }
    attrib.type = type.value();
    attrib.normalized = GL_FALSE;
    attrib.offset = offset;
    offset += (input.bit_width * input.vec_size) / 8;
    vertex_attrib_arrays.emplace_back(attrib);
  }
  for (auto& array : vertex_attrib_arrays) {
    array.stride = offset;
  }
  vertex_attrib_arrays_ = std::move(vertex_attrib_arrays);
  return true;
}

static std::string NormalizeUniformKey(const std::string& key) {
  std::string result;
  result.reserve(key.length());
  for (char ch : key) {
    if (ch != '_') {
      result.push_back(toupper(ch));
    }
  }
  return result;
}

static std::string CreateUniformMemberKey(const std::string& struct_name,
                                          const std::string& member,
                                          bool is_array) {
  std::string result;
  result.reserve(struct_name.length() + member.length() + (is_array ? 4 : 1));
  result += struct_name;
  result += '.';
  result += member;
  if (is_array) {
    result += "[0]";
  }
  return NormalizeUniformKey(result);
}

static std::string CreateUniformMemberKey(
    const std::string& non_struct_member) {
  return NormalizeUniformKey(non_struct_member);
}

bool BufferBindingsGLES::ReadUniformsBindings(const ProcTableGLES& gl,
                                              GLuint program) {
  if (!gl.IsProgram(program)) {
    return false;
  }
  GLint max_name_size = 0;
  gl.GetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &max_name_size);

  GLint uniform_count = 0;
  gl.GetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniform_count);
  for (GLint i = 0; i < uniform_count; i++) {
    std::vector<GLchar> name;
    name.resize(max_name_size);
    GLsizei written_count = 0u;
    GLint uniform_var_size = 0u;
    GLenum uniform_type = GL_FLOAT;
    // Note: Active uniforms are defined as uniforms that may have an impact on
    //       the output of the shader. Drivers are allowed to (and often do)
    //       optimize out unused uniforms.
    gl.GetActiveUniform(program,            // program
                        i,                  // index
                        max_name_size,      // buffer_size
                        &written_count,     // length
                        &uniform_var_size,  // size
                        &uniform_type,      // type
                        name.data()         // name
    );
    auto location = gl.GetUniformLocation(program, name.data());
    if (location == -1) {
      VALIDATION_LOG << "Could not query the location of an active uniform.";
      return false;
    }
    if (written_count <= 0) {
      VALIDATION_LOG << "Uniform name could not be read for active uniform.";
      return false;
    }
    uniform_locations_[NormalizeUniformKey(std::string{
        name.data(), static_cast<size_t>(written_count)})] = location;
  }
  return true;
}

bool BufferBindingsGLES::BindVertexAttributes(const ProcTableGLES& gl,
                                              size_t vertex_offset) const {
  for (const auto& array : vertex_attrib_arrays_) {
    gl.EnableVertexAttribArray(array.index);
    gl.VertexAttribPointer(array.index,       // index
                           array.size,        // size (must be 1, 2, 3, or 4)
                           array.type,        // type
                           array.normalized,  // normalized
                           array.stride,      // stride
                           reinterpret_cast<const GLvoid*>(static_cast<GLsizei>(
                               vertex_offset + array.offset))  // pointer
    );
  }

  return true;
}

bool BufferBindingsGLES::BindUniformData(
    const ProcTableGLES& gl,
    Allocator& transients_allocator,
    const Bindings& vertex_bindings,
    const Bindings& fragment_bindings) const {
  for (const auto& buffer : vertex_bindings.buffers) {
    if (!BindUniformBuffer(gl, transients_allocator, buffer.second)) {
      return false;
    }
  }
  for (const auto& buffer : fragment_bindings.buffers) {
    if (!BindUniformBuffer(gl, transients_allocator, buffer.second)) {
      return false;
    }
  }

  if (!BindTextures(gl, vertex_bindings, ShaderStage::kVertex)) {
    return false;
  }

  if (!BindTextures(gl, fragment_bindings, ShaderStage::kFragment)) {
    return false;
  }

  return true;
}

bool BufferBindingsGLES::UnbindVertexAttributes(const ProcTableGLES& gl) const {
  for (const auto& array : vertex_attrib_arrays_) {
    gl.DisableVertexAttribArray(array.index);
  }
  return true;
}

bool BufferBindingsGLES::BindUniformBuffer(const ProcTableGLES& gl,
                                           Allocator& transients_allocator,
                                           const BufferResource& buffer) const {
  const auto* metadata = buffer.isa;
  if (metadata == nullptr) {
    // Vertex buffer bindings don't have metadata as those definitions are
    // already handled by vertex attrib pointers. Keep going.
    return true;
  }

  auto device_buffer =
      buffer.resource.buffer->GetDeviceBuffer(transients_allocator);
  if (!device_buffer) {
    VALIDATION_LOG << "Device buffer not found.";
    return false;
  }
  const auto& device_buffer_gles = DeviceBufferGLES::Cast(*device_buffer);
  const uint8_t* buffer_ptr =
      device_buffer_gles.GetBufferData() + buffer.resource.range.offset;

  if (metadata->members.empty()) {
    VALIDATION_LOG << "Uniform buffer had no members. This is currently "
                      "unsupported in the OpenGL ES backend. Use a uniform "
                      "buffer block.";
    return false;
  }

  for (const auto& member : metadata->members) {
    if (member.type == ShaderType::kVoid) {
      // Void types are used for padding. We are obviously not going to find
      // mappings for these. Keep going.
      continue;
    }

    size_t element_count = member.array_elements.value_or(1);

    const auto member_key =
        CreateUniformMemberKey(metadata->name, member.name, element_count > 1);
    const auto location = uniform_locations_.find(member_key);
    if (location == uniform_locations_.end()) {
      // The list of uniform locations only contains "active" uniforms that are
      // not optimized out. So this situation is expected to happen when unused
      // uniforms are present in the shader.
      continue;
    }

    size_t element_stride = member.byte_length / element_count;

    auto* buffer_data =
        reinterpret_cast<const GLfloat*>(buffer_ptr + member.offset);

    std::vector<uint8_t> array_element_buffer;
    if (element_count > 1) {
      // When binding uniform arrays, the elements must be contiguous. Copy the
      // uniforms to a temp buffer to eliminate any padding needed by the other
      // backends.
      array_element_buffer.resize(member.size * element_count);
      for (size_t element_i = 0; element_i < element_count; element_i++) {
        std::memcpy(array_element_buffer.data() + element_i * member.size,
                    reinterpret_cast<const char*>(buffer_data) +
                        element_i * element_stride,
                    member.size);
      }
      buffer_data =
          reinterpret_cast<const GLfloat*>(array_element_buffer.data());
    }

    switch (member.type) {
      case ShaderType::kFloat:
        switch (member.size) {
          case sizeof(Matrix):
            gl.UniformMatrix4fv(location->second,  // location
                                element_count,     // count
                                GL_FALSE,          // normalize
                                buffer_data        // data
            );
            continue;
          case sizeof(Vector4):
            gl.Uniform4fv(location->second,  // location
                          element_count,     // count
                          buffer_data        // data
            );
            continue;
          case sizeof(Vector3):
            gl.Uniform3fv(location->second,  // location
                          element_count,     // count
                          buffer_data        // data
            );
            continue;
          case sizeof(Vector2):
            gl.Uniform2fv(location->second,  // location
                          element_count,     // count
                          buffer_data        // data
            );
            continue;
          case sizeof(Scalar):
            gl.Uniform1fv(location->second,  // location
                          element_count,     // count
                          buffer_data        // data
            );
            continue;
        }
      case ShaderType::kBoolean:
      case ShaderType::kSignedByte:
      case ShaderType::kUnsignedByte:
      case ShaderType::kSignedShort:
      case ShaderType::kUnsignedShort:
      case ShaderType::kSignedInt:
      case ShaderType::kUnsignedInt:
      case ShaderType::kSignedInt64:
      case ShaderType::kUnsignedInt64:
      case ShaderType::kAtomicCounter:
      case ShaderType::kUnknown:
      case ShaderType::kVoid:
      case ShaderType::kHalfFloat:
      case ShaderType::kDouble:
      case ShaderType::kStruct:
      case ShaderType::kImage:
      case ShaderType::kSampledImage:
      case ShaderType::kSampler:
        VALIDATION_LOG << "Could not bind uniform buffer data for key: "
                       << member_key;
        return false;
    }
  }
  return true;
}

bool BufferBindingsGLES::BindTextures(const ProcTableGLES& gl,
                                      const Bindings& bindings,
                                      ShaderStage stage) const {
  size_t active_index = 0;
  for (const auto& texture : bindings.textures) {
    const auto& texture_gles = TextureGLES::Cast(*texture.second.resource);
    if (texture.second.isa == nullptr) {
      VALIDATION_LOG << "No metadata found for texture binding.";
      return false;
    }

    const auto uniform_key = CreateUniformMemberKey(texture.second.isa->name);
    auto uniform = uniform_locations_.find(uniform_key);
    if (uniform == uniform_locations_.end()) {
      VALIDATION_LOG << "Could not find uniform for key: " << uniform_key;
      return false;
    }

    //--------------------------------------------------------------------------
    /// Set the active texture unit.
    ///
    if (active_index >= gl.GetCapabilities()->GetMaxTextureUnits(stage)) {
      VALIDATION_LOG << "Texture units specified exceed the capabilities for "
                        "this shader stage.";
      return false;
    }
    gl.ActiveTexture(GL_TEXTURE0 + active_index);

    //--------------------------------------------------------------------------
    /// Bind the texture.
    ///
    if (!texture_gles.Bind()) {
      return false;
    }

    //--------------------------------------------------------------------------
    /// If there is a sampler for the texture at the same index, configure the
    /// bound texture using that sampler.
    ///
    auto sampler = bindings.samplers.find(texture.first);
    if (sampler != bindings.samplers.end()) {
      const auto& sampler_gles = SamplerGLES::Cast(*sampler->second.resource);
      if (!sampler_gles.ConfigureBoundTexture(texture_gles, gl)) {
        return false;
      }
    }

    //--------------------------------------------------------------------------
    /// Set the texture uniform location.
    ///
    gl.Uniform1i(uniform->second, active_index);

    //--------------------------------------------------------------------------
    /// Bump up the active index at binding.
    ///
    active_index++;
  }
  return true;
}

}  // namespace impeller
