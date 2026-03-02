// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/driver/clang_subcommand.h"

#include <string>
#include <vector>

#include "clang/Basic/DiagnosticIDs.h"
#include "clang/Basic/DiagnosticOptions.h"
#include "clang/Driver/Compilation.h"
#include "clang/Driver/Driver.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/CompilerInvocation.h"
#include "clang/Frontend/TextDiagnosticBuffer.h"
#include "clang/Frontend/TextDiagnosticPrinter.h"
#include "clang/FrontendTool/Utils.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/VirtualFileSystem.h"
#include "llvm/TargetParser/Host.h"
#include "toolchain/base/clang_invocation.h"

namespace TinySwift {

// Execute a cc1 frontend invocation in-process.
static auto RunCC1(llvm::ArrayRef<const char*> cc1_args, const char* argv0)
    -> int {
  auto diag_id = llvm::makeIntrusiveRefCnt<clang::DiagnosticIDs>();
  clang::DiagnosticOptions cc1_diag_opts;
  auto* diags_buffer = new clang::TextDiagnosticBuffer;
  clang::DiagnosticsEngine diags(diag_id, cc1_diag_opts, diags_buffer);

  auto invocation = std::make_shared<clang::CompilerInvocation>();
  bool success = clang::CompilerInvocation::CreateFromArgs(
      *invocation, cc1_args, diags, argv0);

  auto compiler =
      std::make_unique<clang::CompilerInstance>(std::move(invocation));

  compiler->createVirtualFileSystem(llvm::vfs::getRealFileSystem(),
                                    diags_buffer);
  compiler->createDiagnostics();
  diags_buffer->FlushDiagnostics(compiler->getDiagnostics());
  if (!success) {
    return 1;
  }

  success = clang::ExecuteCompilerInvocation(compiler.get());
  return success ? 0 : 1;
}

// In-process cc1 execution handler for the Clang driver's CC1Main callback.
static auto ExecuteCC1(llvm::SmallVectorImpl<const char*>& cc1_argv) -> int {
  llvm::cl::ResetAllOptionOccurrences();

  if (cc1_argv.size() > 1 && llvm::StringRef(cc1_argv[1]) == "-cc1") {
    return RunCC1(llvm::ArrayRef(cc1_argv).slice(2), cc1_argv[0]);
  }

  return 1;
}

auto ExecuteCC1InProcess(llvm::ArrayRef<const char*> argv) -> int {
  llvm::cl::ResetAllOptionOccurrences();

  if (argv.size() > 0 && llvm::StringRef(argv[0]) == "-cc1") {
    const char* argv0 = "clang";
    return RunCC1(argv.slice(1), argv0);
  }

  return 1;
}

static constexpr CommandLine::CommandInfo SubcommandInfo = {
    .name = "clang",
    .help = R"""(
Invoke the bundled Clang compiler with TinySwift toolchain defaults.

Arguments after `--` are forwarded directly to Clang.
)""",
};

ClangSubcommand::ClangSubcommand() : DriverSubcommand(SubcommandInfo) {}

auto ClangSubcommand::BuildOptions(CommandLine::CommandBuilder& b) -> void {
  b.AddStringPositionalArg(
      {
          .name = "ARGS",
          .help = "Arguments to forward to Clang.",
      },
      [&](auto& arg_b) {
        arg_b.Required(false);
        arg_b.Append(&args_);
      });
}

auto ClangSubcommand::Run(DriverEnv& driver_env) -> DriverResult {
  // Use the clang_path as the "binary" name for diagnostics, even though
  // we invoke Clang in-process.
  std::string clang_path = driver_env.installation->clang_path().native();

  // Build default args with TinySwift toolchain settings.
  llvm::SmallVector<std::string> default_args;
  default_args.push_back("--start-no-unused-arguments");
  AppendDefaultClangArgs(*driver_env.installation,
                         llvm::sys::getDefaultTargetTriple(), default_args);
  default_args.push_back("--end-no-unused-arguments");

  // Build the full argument list: [clang_path, defaults..., user_args...].
  std::vector<const char*> argv;
  argv.push_back(clang_path.c_str());
  for (const auto& arg : default_args) {
    argv.push_back(arg.c_str());
  }
  // Convert StringRef args to owned strings first, then collect c_str
  // pointers. This avoids use-after-free when the vector reallocates.
  std::vector<std::string> arg_storage;
  arg_storage.reserve(args_.size());
  for (const auto& arg : args_) {
    arg_storage.push_back(arg.str());
  }
  for (const auto& arg : arg_storage) {
    argv.push_back(arg.c_str());
  }

  // Create a Clang driver and run the compilation in-process.
  clang::DiagnosticOptions diag_opts;
  auto diag_ids = llvm::makeIntrusiveRefCnt<clang::DiagnosticIDs>();
  auto* diag_printer =
      new clang::TextDiagnosticPrinter(*driver_env.error_stream, diag_opts);
  clang::DiagnosticsEngine diag_engine(diag_ids, diag_opts, diag_printer);

  clang::driver::Driver driver(clang_path,
                                llvm::sys::getDefaultTargetTriple(),
                                diag_engine);

  // Tell the driver where to find LLVM tools.
  std::string install_dir =
      driver_env.installation->llvm_install_bin().native();
  driver.Dir = install_dir;

  // Enable in-process cc1 execution for single-job compilations.
  // function_ref stores a pointer to the callable, so we need to keep the
  // function pointer alive as a local variable for the duration of usage.
  int (*cc1_main_fn)(llvm::SmallVectorImpl<const char*>&) = ExecuteCC1;
  driver.CC1Main = cc1_main_fn;

  std::unique_ptr<clang::driver::Compilation> compilation(
      driver.BuildCompilation(argv));
  if (!compilation) {
    return {.success = false};
  }

  llvm::SmallVector<std::pair<int, const clang::driver::Command*>> failing;
  int result = driver.ExecuteCompilation(*compilation, failing);
  return {.success = result == 0};
}

}  // namespace TinySwift
