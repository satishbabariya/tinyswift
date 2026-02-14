// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <gtest/gtest.h>

#include "absl/flags/flag.h"
#include "toolchain/diagnostics/kind.h"
#include "toolchain/testing/coverage_helper.h"

ABSL_FLAG(std::string, testdata_manifest, "",
          "A path to a file containing repo-relative names of test files.");

namespace TinySwift::Diagnostics {
namespace {

constexpr Kind Kinds[] = {
#define TINYSWIFT_DIAGNOSTIC_KIND(Name) Kind::Name,
#include "toolchain/diagnostics/kind.def"
};

constexpr Kind UntestedKinds[] = {
    // These exist only for unit tests.
    Kind::TestDiagnostic,
    Kind::TestDiagnosticOnScope,
    Kind::TestDiagnosticNote,

    // Diagnosing erroneous install conditions, but test environments are
    // typically correct.
    Kind::CompilePreludeManifestError,
    Kind::ConfigFailedToReadDigest,
    Kind::ConfigFailedToSetupTarget,
    Kind::DriverInstallInvalid,

    // These diagnose filesystem issues that are hard to unit test.
    Kind::ErrorReadingFile,
    Kind::ErrorStattingFile,
    Kind::FileTooLarge,
    Kind::FailureBuildingRuntimes,
    Kind::FailureRunningClang,
    Kind::FailureRunningClangToLink,

    // These aren't feasible to test with a normal testcase.
    Kind::TooManyTokens,

    // Producing an emit failure may be infeasible.
    Kind::CodeGenUnableToEmit,

    Kind::LanguageServerDiagnosticInWrongFile,
};

// Looks for diagnostic kinds that aren't covered by a file_test.
TEST(Coverage, Kind) {
  Testing::TestKindCoverage(absl::GetFlag(FLAGS_testdata_manifest),
                            R"(^ *// CHECK:STDERR: .* \[(\w+)\]$)",
                            llvm::ArrayRef(Kinds),
                            llvm::ArrayRef(UntestedKinds));
}

}  // namespace
}  // namespace TinySwift::Diagnostics
