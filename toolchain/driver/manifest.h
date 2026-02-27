// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_DRIVER_MANIFEST_H_
#define TINYSWIFT_TOOLCHAIN_DRIVER_MANIFEST_H_

#include <optional>
#include <string>

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"

namespace TinySwift {

// A single target in a tinyswift.toml manifest.
struct ManifestTarget {
  std::string name;
  std::string type;  // "executable" | "library" | "test"
  llvm::SmallVector<std::string> sources;      // glob patterns
  llvm::SmallVector<std::string> link_flags;   // -l flags
  llvm::SmallVector<std::string> dependencies; // target dependencies
};

// A local path dependency.
struct ManifestDependency {
  std::string name;
  std::string path;  // relative to manifest dir
};

// Parsed representation of a tinyswift.toml file.
struct Manifest {
  std::string name;
  std::string version;
  std::string directory;  // directory containing the manifest
  llvm::StringMap<ManifestTarget> targets;
  llvm::SmallVector<ManifestDependency> dependencies;
};

// Parses a tinyswift.toml manifest file at the given path.
// Returns nullopt on parse failure.
auto ParseManifest(llvm::StringRef path) -> std::optional<Manifest>;

// Walks up from `start_dir` looking for a tinyswift.toml file.
// Returns the path if found, nullopt otherwise.
auto FindManifest(llvm::StringRef start_dir) -> std::optional<std::string>;

// Builds a default manifest when no tinyswift.toml exists.
// All *.swift files in `dir` become sources of an executable target named
// after the directory.
auto BuildDefaultManifest(llvm::StringRef dir) -> Manifest;

// Expands glob patterns in source lists relative to the manifest directory.
auto ExpandSourceGlobs(const ManifestTarget& target,
                       llvm::StringRef base_dir)
    -> llvm::SmallVector<std::string>;

}  // namespace TinySwift

#endif  // TINYSWIFT_TOOLCHAIN_DRIVER_MANIFEST_H_
