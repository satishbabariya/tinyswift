// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: testing/file_test/testdata/include_files/recursive.tinyswift
//
// AUTOUPDATE
// TIP: To test this file alone, run:
// TIP:   bazel test //testing/file_test:file_test_base_test --test_arg=--file_tests=testing/file_test/testdata/include_recursive.tinyswift
// TIP: To dump output, run:
// TIP:   bazel run //testing/file_test:file_test_base_test -- --dump_output --file_tests=testing/file_test/testdata/include_recursive.tinyswift

// CHECK:STDOUT: 6 args: `default_args`, `include_recursive.tinyswift`, `c.tinyswift`, `d.tinyswift`, `a.tinyswift`, `b.tinyswift`
// CHECK:STDOUT: c.tinyswift:2: c
// CHECK:STDOUT: d.tinyswift:2: d
// CHECK:STDOUT: a.tinyswift:2: a
// CHECK:STDOUT: b.tinyswift:2: b
