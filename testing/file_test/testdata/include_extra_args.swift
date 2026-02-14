// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// EXTRA-ARGS: foo
// INCLUDE-FILE: testing/file_test/testdata/include_files/extra_args.tinyswift
//
// AUTOUPDATE
// TIP: To test this file alone, run:
// TIP:   bazel test //testing/file_test:file_test_base_test --test_arg=--file_tests=testing/file_test/testdata/include_extra_args.tinyswift
// TIP: To dump output, run:
// TIP:   bazel run //testing/file_test:file_test_base_test -- --dump_output --file_tests=testing/file_test/testdata/include_extra_args.tinyswift

// CHECK:STDOUT: 6 args: `default_args`, `include_extra_args.tinyswift`, `include_files/extra_args.tinyswift`, `bar`, `baz`, `foo`
