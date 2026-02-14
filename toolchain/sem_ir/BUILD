# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

load("//bazel/cc_rules:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "typed_insts",
    srcs = [
        "ids.cpp",
        "inst_kind.cpp",
    ],
    hdrs = [
        "id_kind.h",
        "ids.h",
        "inst_categories.h",
        "inst_kind.h",
        "singleton_insts.h",
        "typed_insts.h",
    ],
    textual_hdrs = ["inst_kind.def"],
    deps = [
        "//common:check",
        "//common:enum_base",
        "//common:ostream",
        "//common:template_string",
        "//common:type_enum",
        "//toolchain/base:canonical_value_store",
        "//toolchain/base:index_base",
        "//toolchain/base:int",
        "//toolchain/base:value_ids",
        "//toolchain/diagnostics:emitter",
        "//toolchain/parse:node_kind",
        "@llvm-project//llvm:Support",
    ],
)

cc_library(
    name = "file",
    srcs = [
        "file.cpp",
        "function.cpp",
        "inst.cpp",
        "type.cpp",
    ],
    hdrs = [
        "file.h",
        "function.h",
        "inst.h",
        "type.h",
    ],
    deps = [
        ":typed_insts",
        "//common:check",
        "//common:error",
        "//common:hashing",
        "//common:map",
        "//common:ostream",
        "//common:raw_string_ostream",
        "//common:struct_reflection",
        "//toolchain/base:block_value_store",
        "//toolchain/base:index_base",
        "//toolchain/base:int",
        "//toolchain/base:kind_switch",
        "//toolchain/base:shared_value_stores",
        "//toolchain/base:value_store",
        "//toolchain/base:yaml",
        "//toolchain/parse:tree",
        "@llvm-project//llvm:Support",
    ],
)
