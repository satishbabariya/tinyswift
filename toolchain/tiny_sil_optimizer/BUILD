# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

load("//bazel/cc_rules:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "tiny_sil_optimizer",
    srcs = [
        "arc_elim.cpp",
        "dce.cpp",
        "definite_init.cpp",
        "mem2reg.cpp",
        "pass_manager.cpp",
        "return_analysis.cpp",
    ],
    hdrs = [
        "pass_manager.h",
    ],
    deps = [
        "//toolchain/tiny_sil",
        "@llvm-project//llvm:Support",
    ],
)
