// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// AUTOUPDATE
// TIP: To test this file alone, run:
// TIP:   bazel test //testing/file_test:file_test_base_test --test_arg=--file_tests=testing/file_test/testdata/multiple_file_refs_in_line.tinyswift
// TIP: To dump output, run:
// TIP:   bazel run //testing/file_test:file_test_base_test -- --dump_output --file_tests=testing/file_test/testdata/multiple_file_refs_in_line.tinyswift
// CHECK:STDOUT: 3 args: `default_args`, `a.tinyswift`, `b.tinyswift`

// --- a.tinyswift
a.tinyswift:1: b.tinyswift:3: a.tinyswift:3: hello
// CHECK:STDOUT: a.tinyswift:[[@LINE-1]]: a.tinyswift:[[@LINE-1]]: b.tinyswift:3: a.tinyswift:[[@LINE+1]]: hello

// --- b.tinyswift


a.tinyswift:1: b.tinyswift:3: a.tinyswift:3: hello
// CHECK:STDOUT: b.tinyswift:[[@LINE-1]]: a.tinyswift:1: b.tinyswift:[[@LINE-1]]: a.tinyswift:3: hello
a.tinyswift:1: b.tinyswift:5: a.tinyswift:3: hello
// CHECK:STDOUT: b.tinyswift:[[@LINE-1]]: a.tinyswift:1: b.tinyswift:[[@LINE-1]]: a.tinyswift:3: hello
a.tinyswift:1: b.tinyswift:7: a.tinyswift:3: hello
// CHECK:STDOUT: b.tinyswift:[[@LINE-1]]: a.tinyswift:1: b.tinyswift:[[@LINE-1]]: a.tinyswift:3: hello
