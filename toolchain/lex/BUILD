# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

load("//bazel/cc_rules:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "testdata",
    srcs = glob(["testdata/**/*.swift"], allow_empty = True),
)

cc_library(
    name = "token_kind",
    srcs = ["token_kind.cpp"],
    hdrs = ["token_kind.h"],
    textual_hdrs = ["token_kind.def"],
    deps = [
        "//common:check",
        "//common:enum_base",
        "@llvm-project//llvm:Support",
    ],
)

cc_library(
    name = "character_set",
    hdrs = ["character_set.h"],
    deps = ["@llvm-project//llvm:Support"],
)

cc_library(
    name = "token_index",
    hdrs = ["token_index.h"],
    deps = [
        ":token_kind",
        "//toolchain/base:index_base",
    ],
)

cc_library(
    name = "token_info",
    hdrs = ["token_info.h"],
    deps = [
        ":token_index",
        ":token_kind",
        "//common:check",
        "//toolchain/base:int",
        "//toolchain/base:value_ids",
    ],
)

cc_library(
    name = "tokenized_buffer",
    srcs = ["tokenized_buffer.cpp"],
    hdrs = ["tokenized_buffer.h"],
    deps = [
        ":character_set",
        ":token_index",
        ":token_info",
        ":token_kind",
        "//common:check",
        "//common:ostream",
        "//common:string_helpers",
        "//toolchain/base:index_base",
        "//toolchain/base:mem_usage",
        "//toolchain/base:shared_value_stores",
        "//toolchain/diagnostics:emitter",
        "//toolchain/source:source_buffer",
        "@llvm-project//llvm:Support",
    ],
)

cc_library(
    name = "dump",
    srcs = ["dump.cpp"],
    hdrs = ["dump.h"],
    deps = [
        ":tokenized_buffer",
        "//common:ostream",
        "//common:raw_string_ostream",
    ],
    # Always link dump methods.
    alwayslink = 1,
)

cc_library(
    name = "lex",
    srcs = ["lex.cpp"],
    hdrs = ["lex.h"],
    deps = [
        ":character_set",
        ":dump",
        ":token_index",
        ":token_info",
        ":token_kind",
        ":tokenized_buffer",
        "//common:check",
        "//common:vlog",
        "//toolchain/base:shared_value_stores",
        "//toolchain/diagnostics:emitter",
        "//toolchain/source:source_buffer",
        "@llvm-project//llvm:Support",
    ],
)
