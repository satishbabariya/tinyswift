// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/full.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// Prelude integration test: String extensions (hashValue, repeated, indexOf, etc).

func testStringExtensions() -> String {
  let s: String = "hello"
  let h: Int = s.hashValue()
  let r: String = s.repeated(3)
  let i: Int = s.indexOf("ll")
  let d: String = s.description()
  return r
}

// CHECK:STDOUT: filename:        prelude_string_extensions.swift
// CHECK:STDOUT:     inst60000008:    {kind: IntType, arg0: signed, arg1: inst7, type: type(TypeType)}
// CHECK:STDOUT:     inst6000001D:    {kind: FunctionDecl, arg0: function60000000, arg1: inst_block60000005, type: type(TypeType)}
