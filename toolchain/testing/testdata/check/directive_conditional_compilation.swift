// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// EXTRA-ARGS: --define=DEBUG
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M108: Conditional compilation — verifies that #if/#else/#endif selects the
// correct branch based on --define flags. With DEBUG defined, the then-branch
// should be processed and the else-branch should be skipped.

func testConditional() -> Int {
  #if DEBUG
  let x = 42
  #else
  let x = 0
  #endif
  return x
}

// The then-branch (let x = 42) is active because DEBUG is defined.
// SemIR should contain the int_value for 42, and a fn_decl.

// CHECK:STDOUT: filename:        directive_conditional_compilation.swift
// CHECK:STDOUT:     inst60000008:    {kind: IntType, arg0: signed, arg1: inst7, type: type(TypeType)}
// CHECK:STDOUT:     inst6000001D:    {kind: FunctionDecl, arg0: function60000000, arg1: inst_block60000005, type: type(TypeType)}
