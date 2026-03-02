// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M73+M74: Cross-file access control — verifies that public declarations are
// visible across files and that internal declarations (the default) are also
// visible in single-module multi-file compilation.

// --- lib.swift

public func publicHelper() -> Int {
  return 100
}

func internalHelper() -> Int {
  return 200
}

// --- main.swift

func testAccess() -> Int {
  // Both public and internal functions are visible across files within
  // the same module.
  return publicHelper() + internalHelper()
}

// CHECK:STDOUT: filename:        lib.swift
// CHECK:STDOUT:     inst60000008:    {kind: IntType, arg0: signed, arg1: inst7, type: type(TypeType)}
// CHECK:STDOUT:     inst6000001D:    {kind: FunctionDecl, arg0: function60000000, arg1: inst_block60000005, type: type(TypeType)}
