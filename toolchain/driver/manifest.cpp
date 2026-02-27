// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/driver/manifest.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include "llvm/ADT/SmallString.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"

namespace TinySwift {

namespace {

// Trims leading and trailing whitespace from a string.
auto Trim(llvm::StringRef s) -> llvm::StringRef {
  return s.trim(" \t\r\n");
}

// Strips quotes from a TOML string value.
auto Unquote(llvm::StringRef s) -> std::string {
  s = Trim(s);
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
    return s.substr(1, s.size() - 2).str();
  }
  return s.str();
}

// Parses a TOML-style array of strings: ["a", "b", "c"]
auto ParseStringArray(llvm::StringRef line)
    -> llvm::SmallVector<std::string> {
  llvm::SmallVector<std::string> result;
  // Find the brackets.
  auto open = line.find('[');
  auto close = line.rfind(']');
  if (open == llvm::StringRef::npos || close == llvm::StringRef::npos ||
      close <= open) {
    return result;
  }
  llvm::StringRef content = line.substr(open + 1, close - open - 1);

  // Split by commas.
  llvm::SmallVector<llvm::StringRef> parts;
  content.split(parts, ',');
  for (auto& part : parts) {
    auto trimmed = Trim(part);
    if (!trimmed.empty()) {
      result.push_back(Unquote(trimmed));
    }
  }
  return result;
}

// Parses an inline table: { path = "../utils" }
auto ParseInlineTable(llvm::StringRef value)
    -> llvm::StringMap<std::string> {
  llvm::StringMap<std::string> result;
  auto open = value.find('{');
  auto close = value.rfind('}');
  if (open == llvm::StringRef::npos || close == llvm::StringRef::npos) {
    return result;
  }
  llvm::StringRef content = value.substr(open + 1, close - open - 1);

  llvm::SmallVector<llvm::StringRef> pairs;
  content.split(pairs, ',');
  for (auto& pair : pairs) {
    auto eq = pair.find('=');
    if (eq != llvm::StringRef::npos) {
      auto key = Trim(pair.substr(0, eq));
      auto val = Unquote(pair.substr(eq + 1));
      result[key] = val;
    }
  }
  return result;
}

}  // namespace

auto ParseManifest(llvm::StringRef path) -> std::optional<Manifest> {
  std::ifstream file(path.str());
  if (!file.is_open()) {
    return std::nullopt;
  }

  Manifest manifest;
  manifest.directory =
      llvm::sys::path::parent_path(path).str();
  if (manifest.directory.empty()) {
    manifest.directory = ".";
  }

  std::string line;
  std::string current_section;
  std::string current_target;

  while (std::getline(file, line)) {
    llvm::StringRef line_ref = Trim(line);

    // Skip empty lines and comments.
    if (line_ref.empty() || line_ref.starts_with("#")) {
      continue;
    }

    // Section header: [package], [targets.main], [dependencies], etc.
    if (line_ref.starts_with("[") && line_ref.ends_with("]")) {
      current_section =
          line_ref.substr(1, line_ref.size() - 2).str();
      current_target.clear();

      // Check for targets.NAME pattern.
      if (llvm::StringRef(current_section).starts_with("targets.")) {
        current_target =
            llvm::StringRef(current_section).substr(8).str();
        if (!manifest.targets.count(current_target)) {
          ManifestTarget target;
          target.name = current_target;
          target.type = "executable";  // default
          manifest.targets[current_target] = std::move(target);
        }
      }
      continue;
    }

    // Key = value.
    auto eq = line_ref.find('=');
    if (eq == llvm::StringRef::npos) {
      continue;
    }

    auto key = Trim(line_ref.substr(0, eq));
    auto value = Trim(line_ref.substr(eq + 1));

    if (current_section == "package") {
      if (key == "name") {
        manifest.name = Unquote(value);
      } else if (key == "version") {
        manifest.version = Unquote(value);
      }
    } else if (!current_target.empty()) {
      auto& target = manifest.targets[current_target];
      if (key == "type") {
        target.type = Unquote(value);
      } else if (key == "sources") {
        target.sources = ParseStringArray(value);
      } else if (key == "link_flags") {
        target.link_flags = ParseStringArray(value);
      } else if (key == "dependencies") {
        target.dependencies = ParseStringArray(value);
      }
    } else if (current_section == "dependencies") {
      // Dependency: name = { path = "..." }
      auto table = ParseInlineTable(value);
      auto path_it = table.find("path");
      if (path_it != table.end()) {
        ManifestDependency dep;
        dep.name = key.str();
        dep.path = path_it->second;
        manifest.dependencies.push_back(std::move(dep));
      }
    }
  }

  return manifest;
}

auto FindManifest(llvm::StringRef start_dir) -> std::optional<std::string> {
  llvm::SmallString<256> dir(start_dir);
  llvm::sys::path::remove_dots(dir, /*remove_dot_dot=*/true);

  // Walk up to 10 levels.
  for (int i = 0; i < 10; ++i) {
    llvm::SmallString<256> manifest_path = dir;
    llvm::sys::path::append(manifest_path, "tinyswift.toml");

    if (llvm::sys::fs::exists(manifest_path)) {
      return manifest_path.str().str();
    }

    // Move to parent.
    auto parent = llvm::sys::path::parent_path(dir);
    if (parent == dir) {
      break;  // Reached root.
    }
    dir = parent;
  }

  return std::nullopt;
}

auto BuildDefaultManifest(llvm::StringRef dir) -> Manifest {
  Manifest manifest;
  manifest.directory = dir.str();

  // Name from directory basename.
  manifest.name = llvm::sys::path::filename(dir).str();
  if (manifest.name.empty()) {
    manifest.name = "main";
  }

  // Find all .swift files in the directory.
  ManifestTarget target;
  target.name = "main";
  target.type = "executable";
  target.sources.push_back("*.swift");

  manifest.targets["main"] = std::move(target);
  return manifest;
}

auto ExpandSourceGlobs(const ManifestTarget& target,
                       llvm::StringRef base_dir)
    -> llvm::SmallVector<std::string> {
  llvm::SmallVector<std::string> result;

  for (const auto& pattern : target.sources) {
    // Simple glob: check if it contains a wildcard.
    if (pattern.find('*') != std::string::npos) {
      // For *.swift pattern, list directory and match.
      std::error_code ec;
      std::string search_dir = base_dir.str();

      // Extract directory prefix from pattern if any.
      auto slash = pattern.rfind('/');
      std::string file_pattern;
      if (slash != std::string::npos) {
        llvm::SmallString<256> sub_dir(base_dir);
        llvm::sys::path::append(sub_dir, pattern.substr(0, slash));
        search_dir = sub_dir.str().str();
        file_pattern = pattern.substr(slash + 1);
      } else {
        file_pattern = pattern;
      }

      // Convert glob pattern to a simple prefix/suffix match.
      auto star = file_pattern.find('*');
      std::string prefix = file_pattern.substr(0, star);
      std::string suffix = file_pattern.substr(star + 1);

      for (llvm::sys::fs::directory_iterator it(search_dir, ec), end;
           it != end && !ec; it.increment(ec)) {
        llvm::StringRef filename =
            llvm::sys::path::filename(it->path());
        if (filename.starts_with(prefix) && filename.ends_with(suffix)) {
          result.push_back(it->path());
        }
      }
    } else {
      // Literal file path.
      llvm::SmallString<256> full_path(base_dir);
      llvm::sys::path::append(full_path, pattern);
      if (llvm::sys::fs::exists(full_path)) {
        result.push_back(full_path.str().str());
      }
    }
  }

  std::sort(result.begin(), result.end());
  return result;
}

}  // namespace TinySwift
