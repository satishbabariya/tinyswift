# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

load("//bazel/cc_rules:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "prebuilt_runtimes",
    srcs = glob(["prebuilt_runtimes_tree/**"], allow_empty = True),
)

cc_library(
    name = "driver",
    srcs = [
        "build_subcommand.cpp",
        "build_subcommand.h",
        "compile_subcommand.cpp",
        "compile_subcommand.h",
        "driver.cpp",
        "driver_env.h",
        "driver_subcommand.cpp",
        "format_subcommand.cpp",
        "format_subcommand.h",
        "language_server_subcommand.cpp",
        "language_server_subcommand.h",
        "manifest.cpp",
        "manifest.h",
        "run_subcommand.cpp",
        "run_subcommand.h",
        "test_subcommand.cpp",
        "test_subcommand.h",
    ],
    hdrs = [
        "driver.h",
        "driver_subcommand.h",
    ],
    textual_hdrs = ["flags.def"],
    deps = [
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
        "//toolchain/codegen:codegen_options",
        "//toolchain/codegen:optimize",
        "//toolchain/codegen:target_machine",
        "//toolchain/diagnostics:emitter",
        "//toolchain/diagnostics:file_diagnostics",
        "//toolchain/diagnostics:sorting_consumer",
        "//toolchain/format",
        "//toolchain/language_server",
        "//toolchain/lex",
        "//toolchain/linker",
        "//toolchain/lower",
        "//toolchain/lower:options",
        "//toolchain/parse",
        "//toolchain/parse:tree",
        "//toolchain/sem_ir:file",
        "//toolchain/sem_ir:typed_insts",
        "//toolchain/source:source_buffer",
        "//toolchain/tiny_sil",
        "//toolchain/tiny_sil_gen",
        "//toolchain/tiny_sil_optimizer",
        "@llvm-project//clang:codegen",
        "@llvm-project//clang:frontend",
        "@llvm-project//llvm:Core",
        "@llvm-project//llvm:MC",
        "@llvm-project//llvm:Support",
        "@llvm-project//llvm:TargetParser",
    ],
)
