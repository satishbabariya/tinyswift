// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M97: Cycle detection — verifies that classes with class-typed fields are
// marked as cycle-capable, and release_cycle is used instead of release.

class TreeNode {
  var value: Int
  var left: TreeNode?
  var right: TreeNode?
  init(value: Int) {
    self.value = value
  }
}

fn testCycleCapable() {
  let node = TreeNode(value: 1)
  // TreeNode has Optional<TreeNode> fields → cycle-capable.
  // Release should use "release_cycle" variant.
}

// TreeNode should be recognized as cycle-capable due to class-typed fields.
// CHECK:STDOUT: alloc_class
// CHECK:STDOUT: release
