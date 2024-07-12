// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <initializer_list>
#include <sstream>
#include <string>

#include "flutter/fml/macros.h"
#include "flutter/fml/mapping.h"
#include "impeller/compiler/include_dir.h"
#include "impeller/compiler/reflector.h"
#include "impeller/compiler/source_options.h"
#include "impeller/compiler/spirv_compiler.h"
#include "impeller/compiler/types.h"
#include "spirv_msl.hpp"
#include "spirv_parser.hpp"

namespace impeller {
namespace compiler {

class Compiler {
 public:
  Compiler(const std::shared_ptr<const fml::Mapping>& source_mapping,
           const SourceOptions& options,
           Reflector::Options reflector_options);

  ~Compiler();

  bool IsValid() const;

  std::shared_ptr<fml::Mapping> GetSPIRVAssembly() const;

  std::shared_ptr<fml::Mapping> GetSLShaderSource() const;

  std::string GetErrorMessages() const;

  const std::vector<std::string>& GetIncludedFileNames() const;

  std::unique_ptr<fml::Mapping> CreateDepfileContents(
      std::initializer_list<std::string> targets) const;

  const Reflector* GetReflector() const;

 private:
  SourceOptions options_;
  std::shared_ptr<fml::Mapping> spirv_assembly_;
  std::shared_ptr<fml::Mapping> sl_mapping_;
  std::stringstream error_stream_;
  std::unique_ptr<Reflector> reflector_;
  std::vector<std::string> included_file_names_;
  bool is_valid_ = false;

  std::string GetSourcePrefix() const;

  std::string GetDependencyNames(const std::string& separator) const;

  FML_DISALLOW_COPY_AND_ASSIGN(Compiler);
};

}  // namespace compiler
}  // namespace impeller
