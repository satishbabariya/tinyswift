// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M73: Multi-file cross-call — verifies that a function defined in one file
// can be called from another file. Both files share a single scope and all
// declarations are visible across files without import statements.

// --- a.swift

func add(_ x: Int, _ y: Int) -> Int {
  return x + y
}

// --- b.swift

func testCrossCall() -> Int {
  return add(3, 4)
}

// Both functions should appear in the combined SemIR output:
//   - add defined in a.swift
//   - testCrossCall defined in b.swift, calling add

// CHECK:STDOUT: FunctionDecl
// CHECK:STDOUT: FunctionDecl
// CHECK:STDOUT: call
