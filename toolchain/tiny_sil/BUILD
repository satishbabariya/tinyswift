# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

load("//bazel/cc_rules:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "tiny_sil",
    srcs = [
        "printer.cpp",
        "verifier.cpp",
    ],
    hdrs = [
        "basic_block.h",
        "function.h",
        "instruction.h",
        "module.h",
        "printer.h",
        "sil_type.h",
        "sil_value.h",
        "verifier.h",
    ],
    deps = [
        "@llvm-project//llvm:Support",
    ],
)
