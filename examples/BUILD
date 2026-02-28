# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

load("@rules_shell//shell:sh_test.bzl", "sh_test")

package(default_visibility = ["//visibility:public"])

filegroup(
    name = "example_files",
    srcs = glob(["*.swift"]),
)

# --- Full-pipeline examples (with func main() entry point) ---
# These examples compile through the entire pipeline: lex → parse → check →
# sil → lower → optimize → codegen → link, and then execute to verify runtime
# correctness.

_EXECUTABLE_EXAMPLES = [
    "binary_search",
    "calculator",
    "fibonacci",
    "linked_list",
    "number_theory",
    "recursion",
    "sorting",
    "stack_and_queue",
    "state_machine",
    "string_algorithms",
]

# --- Syntax/feature demonstration examples (top-level code) ---
# These examples are compiled through the check phase to validate lexing,
# parsing, and semantic analysis of all TinySwift language features.

_FEATURE_EXAMPLES = [
    "01_variables_and_constants",
    "02_basic_types",
    "03_operators",
    "04_functions",
    "05_control_flow",
    "06_switch_and_pattern_matching",
    "07_structs",
    "08_classes",
    "09_enums",
    "10_protocols",
    "11_extensions",
    "12_generics",
    "13_closures",
    "14_error_handling",
    "15_optionals",
    "16_access_control",
    "17_type_casting",
    "18_advanced_features",
    "19_async_and_concurrency",
    "20_generators",
    "21_c_interop",
    "22_conditional_compilation",
]

# Full-pipeline compilation and execution tests for algorithmic examples.
[sh_test(
    name = "example_run_%s" % example,
    size = "medium",
    srcs = ["verify_example.sh"],
    args = [
        "$(location //toolchain:tinyswift)",
        "$(location %s.swift)" % example,
        "run",
    ],
    data = [
        "//toolchain:tinyswift",
        "//toolchain/install:install_data",
    ] + glob(["*.swift"]),
    tags = ["examples"],
) for example in _EXECUTABLE_EXAMPLES]

# Check-phase compilation tests for feature demonstration examples.
[sh_test(
    name = "example_check_%s" % example,
    size = "medium",
    srcs = ["verify_example.sh"],
    args = [
        "$(location //toolchain:tinyswift)",
        "$(location %s.swift)" % example,
        "check",
    ],
    data = [
        "//toolchain:tinyswift",
        "//toolchain/install:install_data",
    ] + glob(["*.swift"]),
    tags = ["examples"],
) for example in _FEATURE_EXAMPLES]

# Full codegen compilation tests for executable examples (compile only, no run).
[sh_test(
    name = "example_codegen_%s" % example,
    size = "medium",
    srcs = ["verify_example.sh"],
    args = [
        "$(location //toolchain:tinyswift)",
        "$(location %s.swift)" % example,
        "codegen",
    ],
    data = [
        "//toolchain:tinyswift",
        "//toolchain/install:install_data",
    ] + glob(["*.swift"]),
    tags = ["examples"],
) for example in _EXECUTABLE_EXAMPLES]

# Comprehensive test that verifies ALL examples through the full pipeline.
sh_test(
    name = "verify_all_examples",
    size = "large",
    timeout = "long",
    srcs = ["verify_all_examples.sh"],
    data = [
        "//toolchain:tinyswift",
        "//toolchain/install:install_data",
    ] + glob(["*.swift"]),
    tags = ["examples"],
)
