// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M78: Basic ARC — verifies that Retain and Release instructions are emitted
// for class-typed variables.

class Dog {
  var name: Int
  init(name: Int) {
    self.name = name
  }
}

fn testCopy() {
  let d = Dog(name: 42)
  let d2 = d
  // d2 is a copy — should emit Retain for d, Release for both at scope exit.
}

// CHECK:STDOUT: retain
// CHECK:STDOUT: release
