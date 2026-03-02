// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/full.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// Prelude integration test: Int extensions (abs, hashValue, isMultiple, description).

func testIntExtensions() -> Int {
  let x: Int = -42
  let a: Int = x.abs()
  let h: Int = x.hashValue()
  let m: Bool = 12.isMultiple(of: 3)
  let d: String = x.description()
  return a
}

// CHECK:STDOUT: filename:        prelude_int_extensions.swift
// CHECK:STDOUT:     inst60000008:    {kind: IntType, arg0: signed, arg1: inst7, type: type(TypeType)}
// CHECK:STDOUT:     inst6000001D:    {kind: FunctionDecl, arg0: function60000000, arg1: inst_block60000005, type: type(TypeType)}
