// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "impeller/compiler/utilities.h"

#include <cctype>
#include <filesystem>
#include <sstream>

namespace impeller {
namespace compiler {

std::string Utf8FromPath(const std::filesystem::path& path) {
  return reinterpret_cast<const char*>(path.u8string().c_str());
}

std::string InferShaderNameFromPath(std::string_view path) {
  auto p = std::filesystem::path{path}.stem();
  return Utf8FromPath(p);
}

std::string ConvertToCamelCase(std::string_view string) {
  if (string.empty()) {
    return "";
  }

  std::stringstream stream;
  bool next_upper = true;
  for (size_t i = 0, count = string.length(); i < count; i++) {
    auto ch = string.data()[i];
    if (next_upper) {
      next_upper = false;
      stream << static_cast<char>(std::toupper(ch));
      continue;
    }
    if (ch == '_') {
      next_upper = true;
      continue;
    }
    stream << ch;
  }
  return stream.str();
}

std::string ConvertToEntrypointName(std::string_view string) {
  if (string.empty()) {
    return "";
  }
  std::stringstream stream;
  // Append a prefix if the first character is not a letter.
  if (!std::isalpha(string.data()[0])) {
    stream << "i_";
  }
  for (size_t i = 0, count = string.length(); i < count; i++) {
    auto ch = string.data()[i];
    if (std::isalnum(ch) || ch == '_') {
      stream << ch;
    }
  }
  return stream.str();
}

}  // namespace compiler
}  // namespace impeller
