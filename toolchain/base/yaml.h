// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_BASE_YAML_H_
#define TINYSWIFT_TOOLCHAIN_BASE_YAML_H_

#include "common/check.h"
#include "common/ostream.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/YAMLTraits.h"

// This file provides adapters for outputting YAML using llvm::yaml's APIs. It
// only supports output, not input. However, it addresses the mix of const and
// non-const expectations of the llvm::yaml that make it difficult to otherwise
// use the trait-based approach.

namespace TinySwift::Yaml {

// Helper for printing YAML, to maintain a consistent configuration.
template <typename T>
inline auto Print(llvm::raw_ostream& out, T yaml) -> void {
  llvm::yaml::Output yout(out, /*Ctxt=*/nullptr, /*WrapColumn=*/80);
  yout << yaml;
}

// Similar to the standard Printable<T>, but relies on OutputYaml for printing.
template <typename T>
class Printable : public TinySwift::Printable<T> {
 public:
  auto Print(llvm::raw_ostream& out) const -> void {
    TinySwift::Yaml::Print(out, static_cast<const T*>(this)->OutputYaml());
  }
};

// Adapts a function for outputting YAML as a scalar. This currently assumes no
// scalars passed through this should be quoted.
class OutputScalar {
 public:
  template <typename T>
  explicit OutputScalar(const T& val)
      : output_([&](llvm::raw_ostream& out) -> void { out << val; }) {}

  explicit OutputScalar(const llvm::APInt& val)
      : output_([&](llvm::raw_ostream& out) -> void {
          // TinySwift's plain APInt storage is typically unsigned.
          val.print(out, /*isSigned=*/false);
        }) {}

  explicit OutputScalar(std::function<auto(llvm::raw_ostream&)->void> output)
      : output_(std::move(output)) {}

  auto Output(llvm::raw_ostream& out) const -> void { output_(out); }

 private:
  std::function<auto(llvm::raw_ostream&)->void> output_;
};

// Adapts a function for outputting YAML as a mapping.
class OutputMapping {
 public:
  class Map {
   public:
    // `io` must not be null.
    explicit Map(llvm::yaml::IO* io) : io_(io) {}

    // Maps a value. This mainly takes responsibility for copying the value,
    // letting mapRequired take `&value`.
    template <typename T>
    auto Add(llvm::StringRef key, T value) -> void {
      io_->mapRequired(key.data(), value);
    }

   private:
    llvm::yaml::IO* io_;
  };

  explicit OutputMapping(std::function<auto(OutputMapping::Map)->void> output)
      : output_(std::move(output)) {}

  auto Output(llvm::yaml::IO& io) -> void { output_(Map(&io)); }

 private:
  std::function<auto(OutputMapping::Map)->void> output_;
};

}  // namespace TinySwift::Yaml

// Link OutputScalar to the llvm::yaml::IO API.
template <>
struct llvm::yaml::ScalarTraits<TinySwift::Yaml::OutputScalar> {
  static auto output(const TinySwift::Yaml::OutputScalar& value, void* /*ctxt*/,
                     llvm::raw_ostream& out) -> void {
    value.Output(out);
  }
  static auto input(StringRef /*scalar*/, void* /*ctxt*/,
                    TinySwift::Yaml::OutputScalar& /*value*/) -> StringRef {
    TINYSWIFT_FATAL("Input is unsupported.");
  }
  static auto mustQuote(StringRef /*value*/) -> QuotingType {
    return QuotingType::None;
  }
};
static_assert(llvm::yaml::has_ScalarTraits<TinySwift::Yaml::OutputScalar>::value);

// Link OutputMapping to the llvm::yaml::IO API.
template <>
struct llvm::yaml::MappingTraits<TinySwift::Yaml::OutputMapping> {
  static auto mapping(IO& io, TinySwift::Yaml::OutputMapping& mapping) -> void {
    mapping.Output(io);
  }
};
static_assert(llvm::yaml::has_MappingTraits<TinySwift::Yaml::OutputMapping,
                                            llvm::yaml::EmptyContext>::value);

#endif  // TINYSWIFT_TOOLCHAIN_BASE_YAML_H_
