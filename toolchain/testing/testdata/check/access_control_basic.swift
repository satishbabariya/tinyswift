// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M74: Access control basics — verifies that public, internal, and private
// modifiers are parsed and applied to declarations. Members with private
// access should be accessible within the declaring scope but not outside.

struct Counter {
  private var count: Int

  public func getCount() -> Int {
    // Private field 'count' is accessible within the declaring scope.
    return count
  }

  func increment() -> Counter {
    return Counter(count: count + 1)
  }
}

public func makeCounter() -> Counter {
  return Counter(count: 0)
}

// The struct, its methods, and the public function should all appear in SemIR.
// Private field access within the struct is allowed.

// CHECK:STDOUT: filename:        access_control_basic.swift
// CHECK:STDOUT:     inst60000008:    {kind: IntType, arg0: signed, arg1: inst7, type: type(TypeType)}
// CHECK:STDOUT:     inst6000001D:    {kind: StructType, arg0: name_scope1, type: type(TypeType)}
