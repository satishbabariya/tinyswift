// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <unistd.h>

#include <cstdlib>
#include <string>

#include "common/bazel_working_dir.h"
#include "common/error.h"
#include "common/init_llvm.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/LLVMDriver.h"
#include "toolchain/base/install_paths.h"
#include "toolchain/driver/clang_subcommand.h"
#include "toolchain/driver/driver.h"
#include "toolchain/install/busybox_info.h"

namespace TinySwift {

// The actual `main` implementation. Can return an exit code or an `Error`
// (which causes EXIT_FAILRUE).
static auto Main(int argc, char** argv) -> ErrorOr<int> {
  InitLLVM init_llvm(argc, argv);

  // Start by resolving any symlinks.
  TINYSWIFT_ASSIGN_OR_RETURN(auto busybox_info, GetBusyboxInfo(argv[0]));

  std::filesystem::path exe_path = busybox_info.bin_path.string();
  exe_path = SetWorkingDirForBazelRun(exe_path);

  const auto install_paths = InstallPaths::MakeExeRelative(exe_path.native());
  if (install_paths.error()) {
    return Error(*install_paths.error());
  }

  // If `LLVM_SYMBOLIZER_PATH` is unset, sets it. Signals.cpp would do some more
  // path resolution which this overrides in favor of using the busybox itself
  // for symbolization.
  setenv(
      "LLVM_SYMBOLIZER_PATH",
      (install_paths.llvm_install_bin().native() + "llvm-symbolizer").c_str(),
      /*overwrite=*/0);

  auto fs = llvm::vfs::getRealFileSystem();

  llvm::SmallVector<llvm::StringRef> raw_args;
  raw_args.append(argv + 1, argv + argc);

  // When the Clang driver re-invokes the busybox for cc1 jobs (out-of-process
  // fallback for multi-input compilations), intercept -cc1 and handle it
  // directly in-process rather than routing through the TinySwift driver.
  if (busybox_info.mode &&
      (*busybox_info.mode == "clang" || *busybox_info.mode == "clang++" ||
       *busybox_info.mode == "clang-cl" || *busybox_info.mode == "clang-cpp") &&
      !raw_args.empty() && raw_args[0] == "-cc1") {
    // Build const char* argv for the cc1 handler.
    llvm::SmallVector<const char*> cc1_argv;
    for (const auto& arg : raw_args) {
      cc1_argv.push_back(arg.data());
    }
    return ExecuteCC1InProcess(cc1_argv);
  }

  llvm::SmallVector<llvm::StringRef> args;
  args.reserve(argc + 1);
  if (busybox_info.mode) {
    // Map busybox modes to the relevant subcommands with any flags needed to
    // emulate the requested command. Typically, our busyboxed binaries redirect
    // to a specific subcommand with some flags set and then pass the remaining
    // busybox arguments as positional arguments to that subcommand.
    //
    auto subcommand_args =
        llvm::StringSwitch<llvm::SmallVector<llvm::StringRef>>(
            *busybox_info.mode)
            // The `clang` program name used configures the default for its
            // `--driver-mode` flag. The first of these is redundant with the
            // default, but we group it here for clarity.
            .Case("clang", {"clang", "--"})
            .Case("clang++", {"clang", "--", "--driver-mode=g++"})
            .Case("clang-cl", {"clang", "--", "--driver-mode=cl"})
            .Case("clang-cpp", {"clang", "--", "--driver-mode=cpp"})

            // LLD has platform-specific program names that we translate into
            // platform flags.
            .Case("ld.lld", {"lld", "--platform=gnu", "--"})
            .Case("ld64.lld", {"lld", "--platform=darwin", "--"})

    // We also support a number of LLVM tools with a trivial translation
    // to subcommands. If any of these end up needing more advanced
    // translation, that can be factored into the `.def` file to provide custom
    // expansion here.
#define TINYSWIFT_LLVM_TOOL(Id, Name, BinName, MainFn) \
  .Case(BinName, {"llvm", Name, "--"})
#include "toolchain/base/llvm_tools.def"

            .Default({*busybox_info.mode, "--"});

    // When we're operating as a busybox, we also support a special command line
    // syntax for passing flags to the base TinySwift driver as
    // `-Xtinyswift=--some-tinyswift-flag=some-value`. Extract any arguments of that
    // form, remove the prefix, and prepend them to the arg list prior to the
    // busybox subcommand arguments.
    llvm::erase_if(raw_args, [&args](llvm::StringRef raw_arg) {
      if (raw_arg.consume_front("-Xtinyswift=")) {
        args.push_back(raw_arg);
        return true;
      }
      return false;
    });

    // And now append the subcommand args.
    args.append(subcommand_args);
  }
  args.append(raw_args);

  Driver driver(fs, &install_paths, stdin, &llvm::outs(), &llvm::errs(),
                /*fuzzing=*/false, /*enable_leaking=*/true);
  bool success = driver.RunCommand(args).success;
  return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

}  // namespace TinySwift

auto main(int argc, char** argv) -> int {
  auto result = TinySwift::Main(argc, argv);
  if (result.ok()) {
    return *result;
  } else {
    llvm::errs() << "error: " << result.error() << "\n";
    return EXIT_FAILURE;
  }
}
