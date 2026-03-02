// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M108: Compile-time diagnostics — verifies that #assert with a true condition
// does not emit an error, and that #warning emits a warning diagnostic.
// Note: #error and #assert(false) would cause compilation failure, so we only
// test the non-fatal paths here.

#assert(true, "this should pass")
#warning("this is a compile-time warning")

func placeholder() -> Int {
  return 1
}

// #assert(true) is a no-op (passes).
// #warning emits a diagnostic to stderr.

// CHECK:STDOUT: filename:        directive_assert_warning_error.swift
// CHECK:STDOUT:     inst60000008:    {kind: IntType, arg0: signed, arg1: inst7, type: type(TypeType)}
// CHECK:STDOUT:     inst6000001D:    {kind: FunctionDecl, arg0: function60000000, arg1: inst_block60000005, type: type(TypeType)}
