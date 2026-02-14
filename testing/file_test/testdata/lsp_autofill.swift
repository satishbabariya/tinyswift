// Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// AUTOUPDATE
// TIP: To test this file alone, run:
// TIP:   bazel test //testing/file_test:file_test_base_test --test_arg=--file_tests=testing/file_test/testdata/lsp_autofill.tinyswift
// TIP: To dump output, run:
// TIP:   bazel run //testing/file_test:file_test_base_test -- --dump_output --file_tests=testing/file_test/testdata/lsp_autofill.tinyswift

// --- foo.tinyswift
class Foo {
  fn foo();
  fn bar() {}
}

// --- STDIN
[[@LSP-NOTIFY:textDocument/didOpen:
  "textDocument": {
    "uri": "file:/foo.tinyswift",
    "languageId": "tinyswift",
    "text": "FROM_FILE_SPLIT"
  }
]]

// --- AUTOUPDATE-SPLIT

// CHECK:STDERR: --- STDIN:
// CHECK:STDERR: Content-Length: 182
// CHECK:STDERR:
// CHECK:STDERR: {"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"languageId":"tinyswift","text":"class Foo {\n  fn foo();\n  fn bar() {}\n}\n\n","uri":"file:/foo.tinyswift"}}}
// CHECK:STDERR:
// CHECK:STDERR:
// CHECK:STDOUT: 2 args: `default_args`, `foo.tinyswift`
// CHECK:STDOUT: foo.tinyswift:1: class Foo {
// CHECK:STDOUT: foo.tinyswift:2:   fn foo();
// CHECK:STDOUT: foo.tinyswift:3:   fn bar() {}
// CHECK:STDOUT: foo.tinyswift:4: }
