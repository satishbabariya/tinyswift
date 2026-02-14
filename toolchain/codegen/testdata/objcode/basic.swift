// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// EXTRA-ARGS: --target=x86_64-unknown-linux-gnu --output=%t
//
// TODO: Add a way to write some basic tests for object file outputs.
// AUTOUPDATE
// TIP: To test this file alone, run:
// TIP:   bazel test //toolchain/testing:file_test --test_arg=--file_tests=toolchain/codegen/testdata/objcode/basic.tinyswift
// TIP: To dump output, run:
// TIP:   bazel run //toolchain/testing:file_test -- --dump_output --file_tests=toolchain/codegen/testdata/objcode/basic.tinyswift

fn Main() {}
