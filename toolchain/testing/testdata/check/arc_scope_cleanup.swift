// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M78: Scope cleanup — verifies that Release instructions are emitted at scope
// exit and before return statements for class-typed locals.

class Node {
  var id: Int
  init(id: Int) {
    self.id = id
  }
}

fn testScopeExit() {
  let a = Node(id: 1)
  let b = Node(id: 2)
  // Both a and b should be released at scope exit in LIFO order (b first).
}

fn testReturnCleanup() -> Int {
  let n = Node(id: 3)
  return 42
  // n should be released before the return.
}

// CHECK:STDOUT: alloc_class
// CHECK:STDOUT: release
// CHECK:STDOUT: release
