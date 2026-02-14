// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// AUTOUPDATE
// TIP: To test this file alone, run:
// TIP:   bazel test //testing/file_test:file_test_base_test --test_arg=--file_tests=testing/file_test/testdata/file_only_re_multi_file.tinyswift
// TIP: To dump output, run:
// TIP:   bazel run //testing/file_test:file_test_base_test -- --dump_output --file_tests=testing/file_test/testdata/file_only_re_multi_file.tinyswift
// CHECK:STDOUT: 3 args: `default_args`, `a.tinyswift`, `b.tinyswift`
// CHECK:STDOUT: unattached message 1

// --- a.tinyswift
// CHECK:STDOUT: file: a.tinyswift
// CHECK:STDOUT: unattached message 2
aaa
// CHECK:STDOUT: line: [[@LINE-1]]: attached message 3
// CHECK:STDOUT: unattached message 4

// CHECK:STDOUT: line: [[@LINE+1]]: late message 5
// CHECK:STDOUT: unattached message 6
// --- b.tinyswift
// CHECK:STDOUT: file: b.tinyswift
bbb
// CHECK:STDOUT: line: [[@LINE-1]]: attached message 7
// CHECK:STDOUT: unattached message 8

// CHECK:STDOUT: line: [[@LINE+1]]: late message 9
// CHECK:STDOUT: unattached message 10
