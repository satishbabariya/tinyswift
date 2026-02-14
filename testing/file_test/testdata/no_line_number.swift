// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// AUTOUPDATE
// TIP: To test this file alone, run:
// TIP:   bazel test //testing/file_test:file_test_base_test --test_arg=--file_tests=testing/file_test/testdata/no_line_number.tinyswift
// TIP: To dump output, run:
// TIP:   bazel run //testing/file_test:file_test_base_test -- --dump_output --file_tests=testing/file_test/testdata/no_line_number.tinyswift
// CHECK:STDOUT: 3 args: `default_args`, `a.tinyswift`, `b.tinyswift`

// --- a.tinyswift
// CHECK:STDOUT: a.tinyswift: msg1
// CHECK:STDOUT: msg2
aaa

// --- b.tinyswift
// CHECK:STDOUT: b.tinyswift: msg3
// CHECK:STDOUT: msg4
// CHECK:STDOUT: a.tinyswift: msg5
bbb
