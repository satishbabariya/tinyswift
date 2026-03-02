// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M76: @cdecl LLVM IR linkage — verifies that @cdecl functions are lowered
// to LLVM IR with the exported C symbol name, external linkage, default
// visibility, and C calling convention.

@cdecl("get_answer")
func answer() -> Int {
  return 42
}

// In LLVM IR:
//   - The function should be defined as @get_answer (C symbol name)
//   - It should have external linkage and default visibility
//   - The function body should return 42

// CHECK:STDOUT: define i64 @get_answer() !dbg !4 {
// CHECK:STDOUT:   ret i64 42, !dbg !7
