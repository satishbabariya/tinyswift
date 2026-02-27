# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

load("//bazel/cc_rules:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "linker",
    srcs = ["link.cpp"],
    hdrs = ["link.h"],
    deps = [
        "//toolchain/base:install_paths",
        "//toolchain/diagnostics:emitter",
        "//toolchain/diagnostics:file_diagnostics",
        "@llvm-project//llvm:Support",
        "@llvm-project//llvm:TargetParser",
    ],
)
