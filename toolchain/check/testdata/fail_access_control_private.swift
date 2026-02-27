// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M74: Private access violation — verifies that accessing a private member
// from outside the declaring scope produces a diagnostic error. The field
// 'secret' is private to Secret and cannot be accessed from testAccess().

struct Secret {
  private var secret: Int

  func reveal() -> Int {
    return secret
  }
}

func testAccess() -> Int {
  let s = Secret(secret: 42)
  return s.secret
}

// Accessing s.secret from outside Secret should produce an error.

// CHECK:STDERR: error
// CHECK:STDERR: private
