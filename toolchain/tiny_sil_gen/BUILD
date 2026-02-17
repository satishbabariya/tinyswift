# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

load("//bazel/cc_rules:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "tiny_sil_gen",
    srcs = [
        "context.cpp",
        "sil_gen.cpp",
    ],
    hdrs = [
        "context.h",
        "managed_value.h",
        "sil_gen.h",
    ],
    deps = [
        "//toolchain/sem_ir:file",
        "//toolchain/sem_ir:typed_insts",
        "//toolchain/tiny_sil",
        "@llvm-project//llvm:Support",
    ],
)
