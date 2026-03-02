// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// ARGS: compile --phase=check --dump-sem-ir --dump-sem-ir-ranges=only --no-prelude-import %s
// NOAUTOUPDATE
// SET-CHECK-SUBSET
//
// M115: Integrated compilation — verifies that the compile subcommand
// processes a simple program through the check phase without errors.
// This validates the driver's argument parsing and pipeline orchestration.

func main() -> Int {
  return 0
}

// CHECK:STDOUT: filename:        compile_emit_object.swift
// CHECK:STDOUT:     inst60000008:    {kind: IntType, arg0: signed, arg1: inst7, type: type(TypeType)}
// CHECK:STDOUT:     inst6000001D:    {kind: FunctionDecl, arg0: function60000000, arg1: inst_block60000005, type: type(TypeType)}
