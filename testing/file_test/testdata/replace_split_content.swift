// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// AUTOUPDATE
// TIP: To test this file alone, run:
// TIP:   bazel test //testing/file_test:file_test_base_test --test_arg=--file_tests=testing/file_test/testdata/replace_split_content.tinyswift
// TIP: To dump output, run:
// TIP:   bazel run //testing/file_test:file_test_base_test -- --dump_output --file_tests=testing/file_test/testdata/replace_split_content.tinyswift
// CHECK:STDOUT: 6 args: `default_args`, `a.tinyswift`, `b.impl.tinyswift`, `todo_c.tinyswift`, `todo_fail_d.tinyswift`, `triplicate.tinyswift`

// --- a.tinyswift

library "[[@TEST_NAME]]";
// CHECK:STDOUT: a.tinyswift:[[@LINE-1]]: library "a";

// --- b.impl.tinyswift

library "[[@TEST_NAME]]";
// CHECK:STDOUT: b.impl.tinyswift:[[@LINE-1]]: library "b";

// --- todo_c.tinyswift

library "[[@TEST_NAME]]";
// CHECK:STDOUT: todo_c.tinyswift:[[@LINE-1]]: library "c";

// --- todo_fail_d.tinyswift

library "[[@TEST_NAME]]";
// CHECK:STDOUT: todo_fail_d.tinyswift:[[@LINE-1]]: library "d";

// --- triplicate.tinyswift

[[@TEST_NAME]][[@TEST_NAME]][[@TEST_NAME]]
// CHECK:STDOUT: triplicate.tinyswift:[[@LINE-1]]: triplicatetriplicatetriplicate
