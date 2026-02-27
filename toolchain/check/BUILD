# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

load("//bazel/cc_rules:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "check",
    srcs = [
        "check.cpp",
        "comptime_eval.cpp",
        "context.cpp",
        "coroutine_transform.cpp",
        "handle_decl.cpp",
        "handle_expr.cpp",
        "handle_function.cpp",
        "handle_generic.cpp",
        "handle_stmt.cpp",
        "handle_type.cpp",
        "handle_type_decl.cpp",
        "synthesis.cpp",
    ],
    hdrs = [
        "check.h",
        "comptime_eval.h",
        "context.h",
        "coroutine_transform.h",
        "handle_decl.h",
        "handle_expr.h",
        "handle_function.h",
        "handle_generic.h",
        "handle_stmt.h",
        "handle_type.h",
        "handle_type_decl.h",
        "synthesis.h",
    ],
    deps = [
        "//common:ostream",
        "//common:vlog",
        "//toolchain/base:shared_value_stores",
        "//toolchain/base:timings",
        "//toolchain/diagnostics:emitter",
        "//toolchain/lex:tokenized_buffer",
        "//toolchain/parse:tree",
        "//toolchain/sem_ir:file",
        "//toolchain/sem_ir:typed_insts",
        "@llvm-project//clang:frontend",
        "@llvm-project//llvm:Core",
        "@llvm-project//llvm:Support",
    ],
)
