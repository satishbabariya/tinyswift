# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

load("//bazel/cc_rules:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "options",
    hdrs = ["options.h"],
    deps = [
        "@llvm-project//llvm:Support",
    ],
)

cc_library(
    name = "lower",
    srcs = [
        "context.cpp",
        "handle_inst.cpp",
        "handle_type.cpp",
        "lower.cpp",
    ],
    hdrs = [
        "context.h",
        "lower.h",
    ],
    deps = [
        ":options",
        "//common:check",
        "//common:vlog",
        "//toolchain/lex:tokenized_buffer",
        "//toolchain/parse:tree",
        "//toolchain/sem_ir:file",
        "//toolchain/tiny_sil",
        "@llvm-project//llvm:BinaryFormat",
        "@llvm-project//llvm:Core",
        "@llvm-project//llvm:Support",
    ],
)
