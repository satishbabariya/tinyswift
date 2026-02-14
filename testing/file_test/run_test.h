// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#ifndef TINYSWIFT_TESTING_FILE_TEST_RUN_TEST_H_
#define TINYSWIFT_TESTING_FILE_TEST_RUN_TEST_H_

#include "common/error.h"
#include "testing/file_test/file_test_base.h"
#include "testing/file_test/test_file.h"

namespace TinySwift::Testing {

// Runs the test, updating `test_file`.
auto RunTestFile(const FileTestBase& test_base, bool dump_output,
                 TestFile& test_file) -> ErrorOr<Success>;

}  // namespace TinySwift::Testing

#endif  // TINYSWIFT_TESTING_FILE_TEST_RUN_TEST_H_
