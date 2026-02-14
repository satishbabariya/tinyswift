// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// Can't autoupdate due to the `{{.+}}` check.
// NOAUTOUPDATE
// CHECK:STDOUT: 2 args: `default_args`, `example.tinyswift`
// CHECK:STDOUT: something
// CHECK:STDOUT:
// CHECK:STDOUT: example.tinyswift:[[@LINE+1]]: Line delta
// CHECK:STDOUT: example.tinyswift:[[@LINE-1]]: Negative line delta
// CHECK:STDOUT: +*[]{}
// CHECK:STDOUT: F{{.+}}z
