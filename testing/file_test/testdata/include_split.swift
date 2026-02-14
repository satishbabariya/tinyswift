// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: testing/file_test/testdata/include_files/split.tinyswift
//
// AUTOUPDATE
// TIP: To test this file alone, run:
// TIP:   bazel test //testing/file_test:file_test_base_test --test_arg=--file_tests=testing/file_test/testdata/include_split.tinyswift
// TIP: To dump output, run:
// TIP:   bazel run //testing/file_test:file_test_base_test -- --dump_output --file_tests=testing/file_test/testdata/include_split.tinyswift

// CHECK:STDOUT: 4 args: `default_args`, `include_split.tinyswift`, `a.tinyswift`, `b.tinyswift`
// CHECK:STDOUT: a.tinyswift:2: a
// CHECK:STDOUT: b.tinyswift:2: b
