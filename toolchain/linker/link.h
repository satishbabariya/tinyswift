// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_LINKER_LINK_H_
#define TINYSWIFT_TOOLCHAIN_LINKER_LINK_H_

#include <string>

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "toolchain/base/install_paths.h"
#include "toolchain/diagnostics/consumer.h"

namespace TinySwift {

// Options controlling the link step.
struct LinkOptions {
  // Path for the output executable or library.
  llvm::StringRef output_path;

  // Object files to link.
  llvm::SmallVector<std::string> object_files;

  // Additional libraries to link (-l flags).
  llvm::SmallVector<std::string> link_libs;

  // Additional library search paths (-L paths).
  llvm::SmallVector<std::string> lib_paths;

  // Strip debug symbols from the output.
  bool strip = false;

  // Enable Thin LTO.
  bool lto_thin = false;

  // Enable full LTO.
  bool lto_full = false;

  // Enable dead code stripping.
  bool dead_strip = true;

  // Produce a position-independent executable.
  bool pie = true;

  // Target triple for cross-compilation.
  std::string target_triple;

  // Whether to produce a static library (.a) instead of an executable.
  bool static_lib = false;
};

// Invokes the system linker (via cc) to produce an executable from object
// files. The runtime library is automatically linked from the install paths.
//
// Returns true on success, false on failure (diagnostics emitted via consumer).
auto InvokeLinker(const InstallPaths& install, const LinkOptions& opts,
                  Diagnostics::Consumer& consumer) -> bool;

// Invokes ar to produce a static archive (.a) from object files.
//
// Returns true on success, false on failure.
auto InvokeArchiver(const LinkOptions& opts,
                    Diagnostics::Consumer& consumer) -> bool;

}  // namespace TinySwift

#endif  // TINYSWIFT_TOOLCHAIN_LINKER_LINK_H_
