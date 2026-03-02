// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// ARGS: compile --phase=check --dump-sem-ir --dump-sem-ir-ranges=only --no-prelude-import %s
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M115+M73: Multi-file compilation via driver — verifies that the driver
// correctly handles multiple source files passed to the compile subcommand.
// All declarations should be visible across files in the shared scope.

// --- lib.swift

func helper() -> Int {
  return 99
}

// --- app.swift

func main() -> Int {
  return helper()
}

// Both functions should appear in combined SemIR output.

// CHECK:STDOUT: filename:        lib.swift
// CHECK:STDOUT:     inst60000008:    {kind: IntType, arg0: signed, arg1: inst7, type: type(TypeType)}
// CHECK:STDOUT:     inst6000001D:    {kind: FunctionDecl, arg0: function60000000, arg1: inst_block60000005, type: type(TypeType)}
