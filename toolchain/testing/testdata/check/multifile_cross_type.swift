// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M73: Multi-file cross-type — verifies that a struct defined in one file
// can be used from another file. The struct type, its fields, and its
// initializer should all be visible across files.

// --- types.swift

struct Point {
  var x: Int
  var y: Int
}

// --- usage.swift

func makePoint() -> Point {
  return Point(x: 10, y: 20)
}

func getX(_ p: Point) -> Int {
  return p.x
}

// types.swift defines the struct; usage.swift uses it for construction and
// field access. Both should appear in the combined SemIR.

// CHECK:STDOUT: filename:        types.swift
// CHECK:STDOUT:     inst60000008:    {kind: IntType, arg0: signed, arg1: inst7, type: type(TypeType)}
// CHECK:STDOUT:     inst6000001D:    {kind: StructType, arg0: name_scope1, type: type(TypeType)}
