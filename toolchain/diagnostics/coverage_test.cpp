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
    Kind::FormatOutputFileError,

    // These aren't feasible to test with a normal testcase.
    Kind::TooManyTokens,

    // Producing an emit failure may be infeasible.
    Kind::CodeGenUnableToEmit,

    Kind::LanguageServerDiagnosticInWrongFile,

    // Driver and subcommand diagnostics not yet covered by file tests.
    Kind::DriverRuntimesCacheInvalid,
    Kind::DriverPrebuiltRuntimesInvalid,
    Kind::DriverCommandLineParseFailed,
    Kind::CompilePhaseFlagConflict,
    Kind::CompileInputNotRegularFile,
    Kind::CompileOutputFileOpenError,
    Kind::CompileTargetInvalid,
    Kind::FormatMultipleFilesToOneOutput,
    Kind::ToolFuzzingDisallowed,
    Kind::CppInteropDriverWarning,
    Kind::CppInteropDriverError,

    // Linking diagnostics.
    Kind::CompileTempFileError,
    Kind::LinkFailed,
    Kind::ArFailed,

    // Build/Run diagnostics.
    Kind::BuildNoSources,
    Kind::BuildDirError,
    Kind::BuildCompileFailed,
    Kind::BuildCircularDep,
    Kind::BuildManifestError,
    Kind::RunCwdError,
    Kind::RunExeNotFound,

    // Test diagnostics.
    Kind::TestCwdError,
    Kind::TestNoFiles,
    Kind::TestNoFunctions,
    Kind::TestBuildDirError,
    Kind::TestHarnessError,
    Kind::TestCompileFailed,

    // Source buffer.
    Kind::ErrorOpeningFile,

    // Lexer diagnostics.
    Kind::UnmatchedOpening,
    Kind::UnmatchedClosing,
    Kind::UnrecognizedCharacters,
    Kind::InvalidCharacterInOperator,
    Kind::InvalidEscapeSequence,
    Kind::UnterminatedString,
    Kind::UnterminatedBlockComment,
    Kind::UnterminatedEscapedIdentifier,
    Kind::InvalidNumberLiteral,
    Kind::InvalidUnicodeEscape,
    Kind::ExpectedDigitAfterPrefix,
    Kind::InvalidDollarIdentifier,
    Kind::NulCharacterInSource,
    Kind::UnexpectedBOM,

    // Parser diagnostics.
    Kind::ExpectedCloseSymbol,
    Kind::ExpectedCloseSymbolParser,
    Kind::ExpectedExpr,
    Kind::ExpectedExprParser,
    Kind::ExpectedCodeBlock,
    Kind::ExpectedCodeBlockBrace,
    Kind::ExpectedConditionExpr,
    Kind::ExpectedStatementExpr,
    Kind::ExpectedDeclSemi,
    Kind::ExpectedDeclSemiOrDefinition,
    Kind::ExpectedDeclName,
    Kind::ExpectedDeclNameParser,
    Kind::ExpectedDeclParser,
    Kind::ExpectedInitializerExpr,
    Kind::ExpectedOpenBrace,
    Kind::ExpectedType,
    Kind::ExpectedPattern,
    Kind::UnrecognizedDecl,

    // Semantics diagnostics.
    Kind::SemanticsTodo,
    Kind::TypeMismatch,
    Kind::RedefinedName,
    Kind::MissingReturn,
    Kind::TooManyArguments,
    Kind::TooFewArguments,
    Kind::CannotCallNonFunction,
    Kind::AmbiguousType,
    Kind::InvalidMemberAccess,
    Kind::CannotInferType,
    Kind::ArgumentLabelMismatch,
    Kind::UnknownStructField,
    Kind::MissingStructField,
    Kind::GenericArgCountMismatch,
    Kind::TypeDoesNotConformToProtocol,
    Kind::CannotInferGenericTypeArgs,
    Kind::PoundWarningMessage,
    Kind::PoundErrorMessage,
    Kind::PoundAssertFailed,
    Kind::ComptimeNonComptimeCall,
    Kind::ComptimeIterationLimit,
    Kind::ComptimeDivisionByZero,
    Kind::ComptimeUnsupportedOperation,
    Kind::ComptimeTypeMismatch,

    // Language server diagnostics.
    Kind::LanguageServerFileUnknown,
    Kind::LanguageServerFileUnsupported,
    Kind::LanguageServerMissingInputStream,
    Kind::LanguageServerNotificationParseError,
    Kind::LanguageServerTransportError,
    Kind::LanguageServerUnexpectedReply,
    Kind::LanguageServerUnsupportedNotification,
    Kind::LanguageServerOpenDuplicateFile,
    Kind::LanguageServerCloseUnknownFile,
    Kind::LanguageServerNotImplemented,
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
