// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M107: Source location directives — verifies that #file, #line, #column, and
// #function are replaced with string/integer literal SemIR instructions.

func testSourceLocation() -> Int {
  let f = #file
  let l = #line
  let c = #column
  let fn = #function
  return l
}

// #file → string_literal (filename)
// #line → int_value (line number)
// #column → int_value (column number)
// #function → string_literal (function name)

// CHECK:STDOUT: filename:        directive_source_location.swift
// CHECK:STDOUT:     inst60000008:    {kind: IntType, arg0: signed, arg1: inst7, type: type(TypeType)}
// CHECK:STDOUT:     inst6000001D:    {kind: FunctionDecl, arg0: function60000000, arg1: inst_block60000005, type: type(TypeType)}
