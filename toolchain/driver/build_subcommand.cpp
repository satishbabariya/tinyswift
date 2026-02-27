// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/driver/build_subcommand.h"

#include <algorithm>
#include <string>
#include <system_error>

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
#include "llvm/TargetParser/Host.h"
#include "toolchain/linker/link.h"
#include "toolchain/driver/manifest.h"

namespace TinySwift {

static constexpr CommandLine::CommandInfo SubcommandInfo = {
    .name = "build",
    .help = R"""(
Build the project.

Compiles and links all source files into an executable. If a tinyswift.toml
manifest file is present, it is used to configure targets and dependencies.
Otherwise, all *.swift files in the current directory are compiled as a
single executable.
)""",
};

BuildSubcommand::BuildSubcommand() : DriverSubcommand(SubcommandInfo) {}

auto BuildCmdOptions::Build(CommandLine::CommandBuilder& b) -> void {
  b.AddFlag(
      {
          .name = "release",
          .help = "Build in release mode (O3, Thin LTO, strip).",
      },
      [&](auto& arg_b) { arg_b.Set(&release); });
  b.AddFlag(
      {
          .name = "verbose",
          .short_name = "v",
          .help = "Print commands as they are executed.",
      },
      [&](auto& arg_b) { arg_b.Set(&verbose); });
}

auto BuildSubcommand::FindAndParseManifest() -> std::optional<Manifest> {
  llvm::SmallString<256> cwd;
  if (llvm::sys::fs::current_path(cwd)) {
    return std::nullopt;
  }

  auto manifest_path = FindManifest(cwd);
  if (manifest_path) {
    return ParseManifest(*manifest_path);
  }

  // No manifest found — build default.
  return BuildDefaultManifest(cwd);
}

auto BuildSubcommand::GetExecutablePath(const Manifest& manifest,
                                        llvm::StringRef build_dir)
    -> std::string {
  llvm::SmallString<256> path(build_dir);
  // Find the first executable target.
  for (const auto& entry : manifest.targets) {
    const auto& target = entry.second;
    if (target.type == "executable") {
      llvm::sys::path::append(path, target.name);
      return path.str().str();
    }
  }
  // Fallback: use manifest name.
  llvm::sys::path::append(path, manifest.name);
  return path.str().str();
}

auto BuildSubcommand::BuildTarget(
    DriverEnv& driver_env, const Manifest& manifest,
    const ManifestTarget& target, llvm::StringRef build_dir,
    const llvm::SmallVector<std::string>& dep_archives) -> bool {
  // Expand source globs.
  auto sources = ExpandSourceGlobs(target, manifest.directory);
  if (sources.empty()) {
    TINYSWIFT_DIAGNOSTIC(BuildNoSources, Error,
                      "no source files found for target `{0}`", std::string);
    driver_env.emitter.Emit(BuildNoSources, target.name);
    return false;
  }

  // Ensure build directory exists.
  if (auto ec = llvm::sys::fs::create_directories(build_dir)) {
    TINYSWIFT_DIAGNOSTIC(BuildDirError, Error,
                      "could not create build directory `{0}`: {1}",
                      std::string, std::string);
    driver_env.emitter.Emit(BuildDirError, build_dir.str(), ec.message());
    return false;
  }

  // Determine output path.
  llvm::SmallString<256> output_path(build_dir);
  if (target.type == "library") {
    llvm::sys::path::append(output_path,
                            "lib" + target.name + ".a");
  } else {
    llvm::sys::path::append(output_path, target.name);
  }

  if (options_.verbose) {
    *driver_env.error_stream << "Building target: " << target.name << "\n";
    for (const auto& src : sources) {
      *driver_env.error_stream << "  " << src << "\n";
    }
  }

  // Multi-file compilation: all sources compiled together into one .o.
  llvm::SmallVector<std::string> object_files;
  {
    llvm::SmallString<256> obj_path(build_dir);
    llvm::sys::path::append(obj_path, target.name + ".o");

    // Find the tinyswift compiler binary.
    auto self = llvm::sys::findProgramByName("tinyswift");
    std::string compiler;
    if (self) {
      compiler = self.get();
    } else if (const char* ts = std::getenv("TINYSWIFT_COMPILER")) {
      compiler = ts;
    } else {
      compiler = "tinyswift";
    }

    llvm::SmallVector<llvm::StringRef> args;
    args.push_back(compiler);
    args.push_back("compile");

    // Add all source files.
    for (const auto& s : sources) {
      args.push_back(s);
    }

    args.push_back("--emit-object");
    args.push_back("--output");
    std::string obj_path_str = obj_path.str().str();
    args.push_back(obj_path_str);

    if (options_.release) {
      args.push_back("--release");
    }

    if (options_.verbose) {
      *driver_env.error_stream << "  compile";
      for (const auto& a : args) {
        *driver_env.error_stream << " " << a;
      }
      *driver_env.error_stream << "\n";
    }

    std::string err_msg;
    int result = llvm::sys::ExecuteAndWait(compiler, args,
                                           /*Env=*/std::nullopt,
                                           /*Redirects=*/{},
                                           /*SecondsToWait=*/0,
                                           /*MemoryLimit=*/0, &err_msg);
    if (result != 0) {
      TINYSWIFT_DIAGNOSTIC(BuildCompileFailed, Error,
                        "compilation failed for target `{0}`", std::string);
      driver_env.emitter.Emit(BuildCompileFailed, target.name);
      return false;
    }

    object_files.push_back(obj_path_str);
  }

  // Link or archive.
  if (target.type == "library") {
    LinkOptions link_opts;
    link_opts.output_path = output_path;
    link_opts.object_files = object_files;
    link_opts.static_lib = true;
    return InvokeArchiver(link_opts, driver_env.consumer);
  }

  LinkOptions link_opts;
  link_opts.output_path = output_path;
  link_opts.object_files = object_files;
  for (const auto& lib : target.link_flags) {
    link_opts.link_libs.push_back(lib);
  }
  // Add dependency archives.
  for (const auto& archive : dep_archives) {
    link_opts.object_files.push_back(archive);
  }
  link_opts.dead_strip = true;
  link_opts.strip = options_.release;
  link_opts.lto_thin = options_.release;
  link_opts.target_triple = llvm::sys::getDefaultTargetTriple();

  return InvokeLinker(*driver_env.installation, link_opts,
                      driver_env.consumer);
}

// M118: Recursively build a dependency and return its archive path.
// `visited` tracks already-built deps to detect circular dependencies.
static auto BuildDependency(BuildSubcommand& builder, DriverEnv& driver_env,
                            const ManifestDependency& dep,
                            llvm::StringRef parent_dir,
                            llvm::StringRef build_dir,
                            llvm::StringSet<>& visited,
                            llvm::SmallVector<std::string>& dep_archives)
    -> bool {
  // Check for circular dependency.
  if (visited.count(dep.name)) {
    TINYSWIFT_DIAGNOSTIC(BuildCircularDep, Error,
                      "circular dependency detected: `{0}`", std::string);
    driver_env.emitter.Emit(BuildCircularDep, dep.name);
    return false;
  }
  visited.insert(dep.name);

  // Resolve path relative to parent manifest directory.
  llvm::SmallString<256> dep_path(parent_dir);
  llvm::sys::path::append(dep_path, dep.path);

  // Look for dependency's manifest.
  llvm::SmallString<256> dep_manifest_path(dep_path);
  llvm::sys::path::append(dep_manifest_path, "tinyswift.toml");

  std::optional<Manifest> dep_manifest;
  if (llvm::sys::fs::exists(dep_manifest_path)) {
    dep_manifest = ParseManifest(dep_manifest_path);
  }
  if (!dep_manifest) {
    // Build default manifest for the dependency.
    dep_manifest = BuildDefaultManifest(dep_path);
    // Default to library type for dependencies.
    for (auto& entry : dep_manifest->targets) {
      entry.second.type = "library";
    }
  }

  // Build dependency's own dependencies first (recursive).
  llvm::SmallVector<std::string> nested_archives;
  for (const auto& nested_dep : dep_manifest->dependencies) {
    if (!BuildDependency(builder, driver_env, nested_dep,
                         dep_manifest->directory, build_dir, visited,
                         nested_archives)) {
      return false;
    }
  }

  // Build the dependency.
  llvm::SmallString<256> dep_build_dir(build_dir);
  llvm::sys::path::append(dep_build_dir, "deps", dep.name);

  auto result = builder.RunBuild(driver_env, *dep_manifest, dep_build_dir);
  if (!result.success) {
    return false;
  }

  // Collect the built archive(s).
  for (const auto& entry : dep_manifest->targets) {
    const auto& target = entry.second;
    if (target.type == "library") {
      llvm::SmallString<256> archive(dep_build_dir);
      llvm::sys::path::append(archive, "lib" + target.name + ".a");
      if (llvm::sys::fs::exists(archive)) {
        dep_archives.push_back(archive.str().str());
      }
    }
  }

  // Also include nested dependency archives.
  for (const auto& na : nested_archives) {
    dep_archives.push_back(na);
  }

  return true;
}

auto BuildSubcommand::RunBuild(DriverEnv& driver_env,
                               const Manifest& manifest,
                               llvm::StringRef build_dir) -> DriverResult {
  // M118: Build dependencies first.
  llvm::SmallVector<std::string> dep_archives;
  llvm::StringSet<> visited;
  for (const auto& dep : manifest.dependencies) {
    if (!BuildDependency(*this, driver_env, dep, manifest.directory,
                         build_dir, visited, dep_archives)) {
      return {.success = false};
    }
  }

  // Topological order: libraries first, then executables.
  llvm::SmallVector<const ManifestTarget*> lib_targets;
  llvm::SmallVector<const ManifestTarget*> exe_targets;
  llvm::SmallVector<const ManifestTarget*> test_targets;

  for (const auto& entry : manifest.targets) {
    const auto& target = entry.second;
    if (target.type == "library") {
      lib_targets.push_back(&target);
    } else if (target.type == "test") {
      test_targets.push_back(&target);
    } else {
      exe_targets.push_back(&target);
    }
  }

  llvm::SmallVector<std::string> built_archives;
  // Include dependency archives.
  for (const auto& da : dep_archives) {
    built_archives.push_back(da);
  }

  // Build libraries first.
  for (const auto* target : lib_targets) {
    if (!BuildTarget(driver_env, manifest, *target, build_dir, {})) {
      return {.success = false};
    }
    llvm::SmallString<256> archive_path(build_dir);
    llvm::sys::path::append(archive_path,
                            "lib" + target->name + ".a");
    built_archives.push_back(archive_path.str().str());
  }

  // Build executables.
  for (const auto* target : exe_targets) {
    if (!BuildTarget(driver_env, manifest, *target, build_dir,
                     built_archives)) {
      return {.success = false};
    }
  }

  return {.success = true};
}

auto BuildSubcommand::Run(DriverEnv& driver_env) -> DriverResult {
  auto manifest = FindAndParseManifest();
  if (!manifest) {
    TINYSWIFT_DIAGNOSTIC(BuildManifestError, Error,
                      "could not find or parse project manifest");
    driver_env.emitter.Emit(BuildManifestError);
    return {.success = false};
  }

  // Build directory.
  llvm::SmallString<256> build_dir(manifest->directory);
  llvm::sys::path::append(build_dir, ".build");
  llvm::sys::path::append(build_dir,
                          options_.release ? "release" : "debug");

  return RunBuild(driver_env, *manifest, build_dir);
}

}  // namespace TinySwift
