# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

load("//bazel/cc_rules:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "codegen",
    srcs = ["codegen.cpp"],
    hdrs = ["codegen.h"],
    deps = [
        "//common:check",
        "//toolchain/diagnostics:emitter",
        "//toolchain/diagnostics:file_diagnostics",
        "@llvm-project//llvm:BitWriter",
        "@llvm-project//llvm:Core",
        "@llvm-project//llvm:Support",
        "@llvm-project//llvm:Target",
    ],
)

cc_library(
    name = "codegen_options",
    srcs = ["codegen_options.cpp"],
    hdrs = ["codegen_options.h"],
    deps = [
        "//common:command_line",
        "@llvm-project//llvm:Support",
        "@llvm-project//llvm:TargetParser",
    ],
)

cc_library(
    name = "optimize",
    srcs = ["optimize.cpp"],
    hdrs = ["optimize.h"],
    deps = [
        "//common:vlog",
        "//toolchain/lower:options",
        "@llvm-project//llvm:Analysis",
        "@llvm-project//llvm:Core",
        "@llvm-project//llvm:Passes",
        "@llvm-project//llvm:Support",
        "@llvm-project//llvm:Target",
    ],
)

cc_library(
    name = "target_machine",
    srcs = ["target_machine.cpp"],
    hdrs = ["target_machine.h"],
    deps = [
        "@llvm-project//llvm:Core",
        "@llvm-project//llvm:MC",
        "@llvm-project//llvm:Support",
        "@llvm-project//llvm:Target",
        "@llvm-project//llvm:TargetParser",
    ],
)
