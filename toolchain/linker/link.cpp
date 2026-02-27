// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/linker/link.h"

#include <string>

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/TargetParser/Triple.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
#include "toolchain/diagnostics/consumer.h"
#include "toolchain/diagnostics/file_diagnostics.h"

namespace TinySwift {

// Finds the C compiler to use as a linker driver.
static auto FindCC() -> std::string {
  // Check CC environment variable first.
  if (const char* cc_env = std::getenv("CC")) {
    return std::string(cc_env);
  }
  // Fall back to system cc.
  if (auto cc_path = llvm::sys::findProgramByName("cc")) {
    return cc_path.get();
  }
  // Try clang explicitly.
  if (auto clang_path = llvm::sys::findProgramByName("clang")) {
    return clang_path.get();
  }
  return "cc";
}

auto InvokeLinker(const InstallPaths& install, const LinkOptions& opts,
                  Diagnostics::Consumer& consumer) -> bool {
  std::string cc = FindCC();

  llvm::SmallVector<llvm::StringRef> args;
  args.push_back(cc);

  // Output path.
  args.push_back("-o");
  std::string output_str = opts.output_path.str();
  args.push_back(output_str);

  // Object files.
  for (const auto& obj : opts.object_files) {
    args.push_back(obj);
  }

  // Runtime library search path.
  auto runtimes_root = install.runtimes_root();
  std::string runtimes_root_str = runtimes_root.string();
  std::string runtime_l_flag;
  if (!runtimes_root_str.empty() &&
      llvm::sys::fs::is_directory(runtimes_root_str)) {
    runtime_l_flag = "-L" + runtimes_root_str;
    args.push_back(runtime_l_flag);
    args.push_back("-ltsruntime");
  }

  // User library search paths.
  llvm::SmallVector<std::string> lib_path_flags;
  for (const auto& path : opts.lib_paths) {
    lib_path_flags.push_back("-L" + path);
  }
  for (const auto& flag : lib_path_flags) {
    args.push_back(flag);
  }

  // User link libraries.
  llvm::SmallVector<std::string> link_lib_flags;
  for (const auto& lib : opts.link_libs) {
    link_lib_flags.push_back("-l" + lib);
  }
  for (const auto& flag : link_lib_flags) {
    args.push_back(flag);
  }

  // Platform-specific dead code stripping.
  if (opts.dead_strip) {
    llvm::Triple triple(opts.target_triple);
    if (triple.isOSDarwin()) {
      args.push_back("-Wl,-dead_strip");
    } else {
      args.push_back("-Wl,--gc-sections");
    }
  }

  // LTO flags.
  if (opts.lto_thin) {
    args.push_back("-flto=thin");
  } else if (opts.lto_full) {
    args.push_back("-flto");
  }

  // Strip debug info.
  if (opts.strip) {
    llvm::Triple triple(opts.target_triple);
    if (triple.isOSDarwin()) {
      args.push_back("-Wl,-S");
    } else {
      args.push_back("-Wl,-s");
    }
  }

  // Linux needs -lm for math functions.
  {
    llvm::Triple triple(opts.target_triple);
    if (triple.isOSLinux()) {
      args.push_back("-lm");
    }
  }

  // Invoke the linker.
  std::string err_msg;
  llvm::SmallVector<llvm::StringRef> arg_refs(args.begin(), args.end());
  int result = llvm::sys::ExecuteAndWait(cc, arg_refs,
                                         /*Env=*/std::nullopt,
                                         /*Redirects=*/{},
                                         /*SecondsToWait=*/0,
                                         /*MemoryLimit=*/0, &err_msg);

  if (result != 0) {
    TINYSWIFT_DIAGNOSTIC(LinkFailed, Error,
                      "linking failed (exit code {0}): {1}", int, std::string);
    Diagnostics::FileEmitter emitter(&consumer);
    emitter.Emit("link", LinkFailed, result, err_msg);
    return false;
  }

  return true;
}

auto InvokeArchiver(const LinkOptions& opts,
                    Diagnostics::Consumer& consumer) -> bool {
  // Find ar.
  std::string ar = "ar";
  if (auto ar_path = llvm::sys::findProgramByName("ar")) {
    ar = ar_path.get();
  }

  llvm::SmallVector<llvm::StringRef> args;
  args.push_back(ar);
  args.push_back("rcs");

  std::string output_str = opts.output_path.str();
  args.push_back(output_str);

  for (const auto& obj : opts.object_files) {
    args.push_back(obj);
  }

  std::string err_msg;
  int result = llvm::sys::ExecuteAndWait(ar, args,
                                         /*Env=*/std::nullopt,
                                         /*Redirects=*/{},
                                         /*SecondsToWait=*/0,
                                         /*MemoryLimit=*/0, &err_msg);

  if (result != 0) {
    TINYSWIFT_DIAGNOSTIC(ArFailed, Error,
                      "archiving failed (exit code {0}): {1}", int,
                      std::string);
    Diagnostics::FileEmitter emitter(&consumer);
    emitter.Emit("archive", ArFailed, result, err_msg);
    return false;
  }

  return true;
}

}  // namespace TinySwift
