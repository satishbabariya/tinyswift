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
    name = "node_category",
    srcs = ["node_category.cpp"],
    hdrs = ["node_category.h"],
    deps = [
        "//common:enum_mask_base",
        "//common:ostream",
        "@llvm-project//llvm:Support",
    ],
)

cc_library(
    name = "node_kind",
    srcs = ["node_kind.cpp"],
    hdrs = [
        "node_ids.h",
        "node_kind.h",
        "typed_nodes.h",
    ],
    textual_hdrs = ["node_kind.def"],
    deps = [
        ":node_category",
        "//common:check",
        "//common:enum_base",
        "//common:ostream",
        "//toolchain/base:index_base",
        "//toolchain/lex:token_index",
        "//toolchain/lex:token_kind",
        "@llvm-project//llvm:Support",
    ],
)

cc_library(
    name = "tree",
    srcs = [
        "extract.cpp",
        "tree.cpp",
        "tree_and_subtrees.cpp",
    ],
    hdrs = [
        "tree.h",
        "tree_and_subtrees.h",
    ],
    deps = [
        ":node_kind",
        "//common:check",
        "//common:error",
        "//common:find",
        "//common:ostream",
        "//common:struct_reflection",
        "//toolchain/base:fixed_size_value_store",
        "//toolchain/base:value_store",
        "//toolchain/lex:token_index",
        "//toolchain/lex:tokenized_buffer",
        "@llvm-project//llvm:Support",
    ],
)

cc_library(
    name = "dump",
    srcs = ["dump.cpp"],
    hdrs = ["dump.h"],
    deps = [
        ":tree",
        "//common:ostream",
        "//common:raw_string_ostream",
        "//toolchain/lex:dump",
    ],
    # Always link dump methods.
    alwayslink = 1,
)

cc_library(
    name = "parse",
    srcs = [
        "context.cpp",
        "parse.cpp",
        "parse_decl.cpp",
        "parse_expr.cpp",
        "parse_pattern.cpp",
        "parse_stmt.cpp",
        "parse_type.cpp",
    ],
    hdrs = [
        "context.h",
        "parse.h",
    ],
    deps = [
        ":dump",
        ":node_kind",
        ":tree",
        "//common:check",
        "//common:ostream",
        "//common:vlog",
        "//toolchain/diagnostics:emitter",
        "//toolchain/lex:token_index",
        "//toolchain/lex:token_kind",
        "//toolchain/lex:tokenized_buffer",
    ],
)
