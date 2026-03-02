// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M111: Derive CustomStringConvertible — verifies that structs conforming to
// CustomStringConvertible get a synthesized description computed property.
// The property builds a string from the type name, field names, and field values.

struct Rectangle: CustomStringConvertible {
  var width: Int
  var height: Int
}

func testDescription() -> String {
  let r = Rectangle(width: 10, height: 20)
  return r.description
}

// Synthesized description property: string_literal for type/field names,
// string_concat to join them, int_to_string for field values.

// CHECK:STDOUT: filename:        derive_description.swift
// CHECK:STDOUT:     inst60000008:    {kind: IntType, arg0: signed, arg1: inst7, type: type(TypeType)}
// CHECK:STDOUT:     inst6000001D:    {kind: StructType, arg0: name_scope1, type: type(TypeType)}
