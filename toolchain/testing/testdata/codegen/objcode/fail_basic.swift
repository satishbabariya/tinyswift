// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// EXTRA-ARGS: --target=x86_64-unknown-linux-gnu --output=%t
//
// TODO: Add a way to write some basic tests for object file outputs.
// NOAUTOUPDATE
// SET-CHECK-SUBSET
// CHECK:STDERR: fail_basic.swift:13:1: error: use of undefined name 'fn' [UndefinedName]

fn Main() {}
