# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

load("//bazel/cc_rules:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "testdata",
    srcs = glob(["testdata/**/*.swift"], allow_empty = True),
)

filegroup(
    name = "prebuilt_runtimes",
    srcs = glob(["prebuilt_runtimes_tree/**"], allow_empty = True),
)

cc_library(
    name = "codegen_options",
    srcs = [
        "codegen_options.cpp",
    ],
    hdrs = [
        "codegen_options.h",
    ],
    deps = [
        "//common:command_line",
        "@llvm-project//llvm:Support",
        "@llvm-project//llvm:TargetParser",
    ],
)

cc_library(
    name = "driver",
    srcs = [
        "compile_subcommand.cpp",
        "compile_subcommand.h",
        "driver.cpp",
        "driver_env.h",
        "driver_subcommand.cpp",
    ],
    hdrs = [
        "driver.h",
        "driver_subcommand.h",
    ],
    textual_hdrs = ["flags.def"],
    deps = [
        ":codegen_options",
        "//common:check",
        "//common:command_line",
        "//common:error",
        "//common:ostream",
        "//common:pretty_stack_trace_function",
        "//common:version",
        "//common:vlog",
        "//toolchain/base:clang_invocation",
        "//toolchain/base:fixed_size_value_store",
        "//toolchain/base:install_paths",
        "//toolchain/base:shared_value_stores",
        "//toolchain/base:timings",
        "//toolchain/check",
        "//toolchain/codegen",
        "//toolchain/diagnostics:emitter",
        "//toolchain/diagnostics:sorting_consumer",
        "//toolchain/lex",
        "//toolchain/lower",
        "//toolchain/lower:options",
        "//toolchain/parse",
        "//toolchain/parse:tree",
        "//toolchain/sem_ir:file",
        "//toolchain/sem_ir:typed_insts",
        "//toolchain/source:source_buffer",
        "@llvm-project//clang:codegen",
        "@llvm-project//clang:frontend",
        "@llvm-project//llvm:Core",
        "@llvm-project//llvm:MC",
        "@llvm-project//llvm:Passes",
        "@llvm-project//llvm:Support",
        "@llvm-project//llvm:TargetParser",
    ],
)
