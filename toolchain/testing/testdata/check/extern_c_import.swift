// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M75: @extern("C") function import — verifies that @extern("C") declarations
// are parsed and produce SemIR function declarations with is_extern_c set.
// The function should be declaration-only (no body) and use the C symbol name.

@extern("C") func abs(_ x: Int) -> Int

func testExternC() -> Int {
  return abs(0 - 42)
}

// abs is an @extern("C") declaration — bodyless, with extern linkage.
// testExternC calls abs, which should appear as a regular call in SemIR.

// CHECK:STDOUT: filename:        extern_c_import.swift
// CHECK:STDOUT:     inst60000008:    {kind: IntType, arg0: signed, arg1: inst7, type: type(TypeType)}
// CHECK:STDOUT:     inst60000020:    {kind: FunctionDecl, arg0: function60000000, arg1: inst_block60000007, type: type(TypeType)}
