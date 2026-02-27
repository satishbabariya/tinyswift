// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// INCLUDE-FILE: toolchain/testing/testdata/min_prelude/none.tinyswift
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M73: Multi-file forward reference — verifies that file ordering does not
// matter. File b.swift calls a function defined in a.swift, and a.swift calls
// a function defined in b.swift. Both directions work because pass 1b
// pre-registers all function stubs before pass 2 processes bodies.

// --- a.swift

func foo() -> Int {
  return bar() + 1
}

// --- b.swift

func bar() -> Int {
  return 42
}

func testForwardRef() -> Int {
  return foo()
}

// foo() calls bar() (forward reference from a.swift to b.swift).
// testForwardRef() calls foo() (back reference from b.swift to a.swift).

// CHECK:STDOUT: FunctionDecl
// CHECK:STDOUT: FunctionDecl
// CHECK:STDOUT: FunctionDecl
// CHECK:STDOUT: call
