// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TOOLCHAIN_DRIVER_TEST_SUBCOMMAND_H_
#define TINYSWIFT_TOOLCHAIN_DRIVER_TEST_SUBCOMMAND_H_

#include "common/command_line.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "toolchain/driver/driver_env.h"
#include "toolchain/driver/driver_subcommand.h"

namespace TinySwift {

struct TestOptions {
  auto Build(CommandLine::CommandBuilder& b) -> void;

  bool verbose = false;
  bool release = false;
  llvm::StringRef filter;  // Optional test file filter.
};

class TestSubcommand : public DriverSubcommand {
 public:
  explicit TestSubcommand();

  auto BuildOptions(CommandLine::CommandBuilder& b) -> void override {
    options_.Build(b);
  }

  auto Run(DriverEnv& driver_env) -> DriverResult override;

 private:
  // Discover test files (*_test.swift, test_*.swift) in the directory.
  auto DiscoverTestFiles(llvm::StringRef dir)
      -> llvm::SmallVector<std::string>;

  // Scan a test file for `func test*()` function names.
  auto DiscoverTestFunctions(llvm::StringRef filepath)
      -> llvm::SmallVector<std::string>;

  // Generate a harness .swift file that calls all discovered test functions.
  auto GenerateHarness(llvm::StringRef output_path,
                       const llvm::SmallVector<std::string>& test_funcs)
      -> bool;

  TestOptions options_;
};

}  // namespace TinySwift

#endif  // TINYSWIFT_TOOLCHAIN_DRIVER_TEST_SUBCOMMAND_H_
