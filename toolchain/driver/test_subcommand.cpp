// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "toolchain/driver/test_subcommand.h"

#include <algorithm>
#include <fstream>
#include <string>

#include "llvm/ADT/SmallString.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Program.h"
#include "llvm/TargetParser/Host.h"
#include "toolchain/linker/link.h"
#include "toolchain/driver/manifest.h"

namespace TinySwift {

static constexpr CommandLine::CommandInfo SubcommandInfo = {
    .name = "test",
    .help = R"""(
Discover and run tests.

Finds test files (*_test.swift, test_*.swift), discovers test functions
(func test*()), generates a test harness, compiles, and runs the tests.
)""",
};

TestSubcommand::TestSubcommand() : DriverSubcommand(SubcommandInfo) {}

auto TestOptions::Build(CommandLine::CommandBuilder& b) -> void {
  b.AddFlag(
      {
          .name = "verbose",
          .short_name = "v",
          .help = "Print verbose test output.",
      },
      [&](auto& arg_b) { arg_b.Set(&verbose); });
  b.AddFlag(
      {
          .name = "release",
          .help = "Build tests in release mode.",
      },
      [&](auto& arg_b) { arg_b.Set(&release); });
  b.AddStringOption(
      {
          .name = "filter",
          .value_name = "PATTERN",
          .help = "Only run tests matching the given pattern.",
      },
      [&](auto& arg_b) { arg_b.Set(&filter); });
}

auto TestSubcommand::DiscoverTestFiles(llvm::StringRef dir)
    -> llvm::SmallVector<std::string> {
  llvm::SmallVector<std::string> test_files;
  std::error_code ec;
  for (llvm::sys::fs::directory_iterator it(dir, ec), end;
       it != end && !ec; it.increment(ec)) {
    llvm::StringRef filename = llvm::sys::path::filename(it->path());
    if (!filename.ends_with(".swift")) {
      continue;
    }
    // Match *_test.swift or test_*.swift patterns.
    if (filename.ends_with("_test.swift") ||
        filename.starts_with("test_")) {
      if (options_.filter.empty() ||
          filename.contains(options_.filter)) {
        test_files.push_back(it->path());
      }
    }
  }
  std::sort(test_files.begin(), test_files.end());
  return test_files;
}

auto TestSubcommand::DiscoverTestFunctions(llvm::StringRef filepath)
    -> llvm::SmallVector<std::string> {
  llvm::SmallVector<std::string> test_funcs;

  std::ifstream file(filepath.str());
  if (!file.is_open()) {
    return test_funcs;
  }

  // Simple line scanner for `func test*()` patterns.
  // Skips comment lines to avoid false positives.
  std::string line;
  while (std::getline(file, line)) {
    llvm::StringRef line_ref = llvm::StringRef(line).ltrim();
    // Skip single-line comments.
    if (line_ref.starts_with("//")) {
      continue;
    }
    // Find "func " followed by "test".
    auto func_pos = line_ref.find("func ");
    if (func_pos == llvm::StringRef::npos) {
      continue;
    }
    auto rest = line_ref.substr(func_pos + 5).ltrim();
    if (!rest.starts_with("test")) {
      continue;
    }
    // Extract the function name up to '('.
    auto paren_pos = rest.find('(');
    if (paren_pos == llvm::StringRef::npos) {
      continue;
    }
    auto name = rest.substr(0, paren_pos).rtrim();
    if (!name.empty()) {
      test_funcs.push_back(name.str());
    }
  }

  return test_funcs;
}

auto TestSubcommand::GenerateHarness(
    llvm::StringRef output_path,
    const llvm::SmallVector<std::string>& test_funcs) -> bool {
  std::ofstream file(output_path.str());
  if (!file.is_open()) {
    return false;
  }

  file << "// Auto-generated test harness.\n\n";

  // Import the runtime test helper.
  file << "@extern(\"C\") func __tinyswift_run_test("
       << "_ fn: () -> Void) -> Int\n\n";

  file << "func main() -> Int {\n";
  file << "  var passed: Int = 0\n";
  file << "  var failed: Int = 0\n";
  file << "  let total: Int = " << test_funcs.size() << "\n";
  file << "  print(\"Running \" + __tinyswift_int_to_string(total) + "
       << "\" tests...\")\n\n";

  for (const auto& func : test_funcs) {
    file << "  print(\"  " << func << " ... \")\n";
    file << "  if __tinyswift_run_test(" << func << ") == 1 {\n";
    file << "    passed = passed + 1\n";
    file << "    print(\"PASS\")\n";
    file << "  } else {\n";
    file << "    failed = failed + 1\n";
    file << "    print(\"FAIL\")\n";
    file << "  }\n\n";
  }

  file << "  print(__tinyswift_int_to_string(passed) + \" passed, \" + "
       << "__tinyswift_int_to_string(failed) + \" failed\")\n";
  file << "  if failed > 0 {\n";
  file << "    return 1\n";
  file << "  }\n";
  file << "  return 0\n";
  file << "}\n";

  return true;
}

auto TestSubcommand::Run(DriverEnv& driver_env) -> DriverResult {
  // Find project root.
  llvm::SmallString<256> cwd;
  if (llvm::sys::fs::current_path(cwd)) {
    TINYSWIFT_DIAGNOSTIC(TestCwdError, Error,
                      "could not determine current directory");
    driver_env.emitter.Emit(TestCwdError);
    return {.success = false};
  }

  // Check for manifest.
  auto manifest_path = FindManifest(cwd);
  std::string project_dir = cwd.str().str();
  if (manifest_path) {
    auto manifest = ParseManifest(*manifest_path);
    if (manifest) {
      project_dir = manifest->directory;
    }
  }

  // Discover test files.
  auto test_files = DiscoverTestFiles(project_dir);
  if (test_files.empty()) {
    TINYSWIFT_DIAGNOSTIC(TestNoFiles, Error,
                      "no test files found (expected *_test.swift or "
                      "test_*.swift)");
    driver_env.emitter.Emit(TestNoFiles);
    return {.success = false};
  }

  // Discover test functions from all test files.
  llvm::SmallVector<std::string> all_test_funcs;
  for (const auto& test_file : test_files) {
    auto funcs = DiscoverTestFunctions(test_file);
    for (auto& f : funcs) {
      all_test_funcs.push_back(std::move(f));
    }
  }

  if (all_test_funcs.empty()) {
    TINYSWIFT_DIAGNOSTIC(TestNoFunctions, Error,
                      "no test functions found (expected func test*())");
    driver_env.emitter.Emit(TestNoFunctions);
    return {.success = false};
  }

  if (options_.verbose) {
    *driver_env.error_stream << "Found " << all_test_funcs.size()
                             << " test function(s) in "
                             << test_files.size() << " file(s)\n";
  }

  // Create build directory.
  llvm::SmallString<256> build_dir(project_dir);
  llvm::sys::path::append(build_dir, ".build", "test");
  if (auto ec = llvm::sys::fs::create_directories(build_dir)) {
    TINYSWIFT_DIAGNOSTIC(TestBuildDirError, Error,
                      "could not create test build directory: {0}",
                      std::string);
    driver_env.emitter.Emit(TestBuildDirError, ec.message());
    return {.success = false};
  }

  // Generate test harness.
  llvm::SmallString<256> harness_path(build_dir);
  llvm::sys::path::append(harness_path, "__test_harness.swift");
  if (!GenerateHarness(harness_path, all_test_funcs)) {
    TINYSWIFT_DIAGNOSTIC(TestHarnessError, Error,
                      "could not generate test harness");
    driver_env.emitter.Emit(TestHarnessError);
    return {.success = false};
  }

  // Collect all source files (non-test sources + test files + harness).
  llvm::SmallVector<std::string> all_sources;

  // Find non-test source files.
  {
    std::error_code ec;
    for (llvm::sys::fs::directory_iterator it(project_dir, ec), end;
         it != end && !ec; it.increment(ec)) {
      llvm::StringRef filename = llvm::sys::path::filename(it->path());
      if (!filename.ends_with(".swift")) {
        continue;
      }
      // Skip test files (they're added separately) and harness.
      if (filename.ends_with("_test.swift") ||
          filename.starts_with("test_") ||
          filename.starts_with("__test_harness")) {
        continue;
      }
      all_sources.push_back(it->path());
    }
  }
  std::sort(all_sources.begin(), all_sources.end());

  // Add test files and harness.
  for (const auto& tf : test_files) {
    all_sources.push_back(tf);
  }
  all_sources.push_back(harness_path.str().str());

  // Compile all together.
  llvm::SmallString<256> obj_path(build_dir);
  llvm::sys::path::append(obj_path, "test_runner.o");

  auto self = llvm::sys::findProgramByName("tinyswift");
  std::string compiler;
  if (self) {
    compiler = self.get();
  } else if (const char* ts = std::getenv("TINYSWIFT_COMPILER")) {
    compiler = ts;
  } else {
    compiler = "tinyswift";
  }

  llvm::SmallVector<llvm::StringRef> compile_args;
  compile_args.push_back(compiler);
  compile_args.push_back("compile");
  for (const auto& src : all_sources) {
    compile_args.push_back(src);
  }
  compile_args.push_back("--emit-object");
  compile_args.push_back("--output");
  std::string obj_str = obj_path.str().str();
  compile_args.push_back(obj_str);
  if (options_.release) {
    compile_args.push_back("--release");
  }

  if (options_.verbose) {
    *driver_env.error_stream << "Compiling tests...\n";
  }

  std::string err_msg;
  int compile_result = llvm::sys::ExecuteAndWait(
      compiler, compile_args, /*Env=*/std::nullopt, /*Redirects=*/{},
      /*SecondsToWait=*/0, /*MemoryLimit=*/0, &err_msg);
  if (compile_result != 0) {
    TINYSWIFT_DIAGNOSTIC(TestCompileFailed, Error,
                      "test compilation failed");
    driver_env.emitter.Emit(TestCompileFailed);
    return {.success = false};
  }

  // Link test executable.
  llvm::SmallString<256> exe_path(build_dir);
  llvm::sys::path::append(exe_path, "test_runner");

  LinkOptions link_opts;
  link_opts.output_path = exe_path;
  link_opts.object_files.push_back(obj_str);
  link_opts.dead_strip = true;
  link_opts.target_triple = llvm::sys::getDefaultTargetTriple();

  if (!InvokeLinker(*driver_env.installation, link_opts,
                    driver_env.consumer)) {
    return {.success = false};
  }

  // Run the test executable.
  if (options_.verbose) {
    *driver_env.error_stream << "Running tests...\n";
  }

  llvm::SmallVector<llvm::StringRef> run_args;
  std::string exe_str = exe_path.str().str();
  run_args.push_back(exe_str);

  int test_result = llvm::sys::ExecuteAndWait(
      exe_str, run_args, /*Env=*/std::nullopt, /*Redirects=*/{},
      /*SecondsToWait=*/0, /*MemoryLimit=*/0, &err_msg);

  return {.success = test_result == 0};
}

}  // namespace TinySwift
