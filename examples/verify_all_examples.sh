#!/bin/bash
# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Comprehensive end-to-end verification of all TinySwift examples.
# Validates the complete compiler pipeline for every example file.

set -euo pipefail

# Find the tinyswift binary relative to the runfiles.
TINYSWIFT=""
for candidate in \
    "${RUNFILES_DIR:-}/_main/toolchain/tinyswift" \
    "${TEST_SRCDIR:-}/_main/toolchain/tinyswift" \
    "$(dirname "$0")/../toolchain/tinyswift"; do
  if [ -x "${candidate}" ]; then
    TINYSWIFT="${candidate}"
    break
  fi
done

if [ -z "${TINYSWIFT}" ]; then
  echo "ERROR: Could not find tinyswift binary"
  exit 1
fi

# Find examples directory.
EXAMPLES_DIR=""
for candidate in \
    "${RUNFILES_DIR:-}/_main/examples" \
    "${TEST_SRCDIR:-}/_main/examples" \
    "$(dirname "$0")"; do
  if [ -d "${candidate}" ] && ls "${candidate}"/*.swift >/dev/null 2>&1; then
    EXAMPLES_DIR="${candidate}"
    break
  fi
done

if [ -z "${EXAMPLES_DIR}" ]; then
  echo "ERROR: Could not find examples directory"
  exit 1
fi

TMPDIR="$(mktemp -d)"
trap 'rm -rf "${TMPDIR}"' EXIT

PASS=0
FAIL=0
ERRORS=""

# Feature examples (top-level code) — verify through check phase.
FEATURE_EXAMPLES=(
  01_variables_and_constants
  02_basic_types
  03_operators
  04_functions
  05_control_flow
  06_switch_and_pattern_matching
  07_structs
  08_classes
  09_enums
  10_protocols
  11_extensions
  12_generics
  13_closures
  14_error_handling
  15_optionals
  16_access_control
  17_type_casting
  18_advanced_features
  19_async_and_concurrency
  20_generators
  21_c_interop
  22_conditional_compilation
)

# Executable examples (with main()) — verify full pipeline.
EXECUTABLE_EXAMPLES=(
  binary_search
  calculator
  fibonacci
  linked_list
  number_theory
  recursion
  sorting
  stack_and_queue
  state_machine
  string_algorithms
)

echo "========================================"
echo "TinySwift Example Verification Suite"
echo "========================================"
echo ""

# Phase 1: Check-phase verification for feature examples.
echo "--- Phase 1: Feature Examples (check phase) ---"
for example in "${FEATURE_EXAMPLES[@]}"; do
  SOURCE="${EXAMPLES_DIR}/${example}.swift"
  if [ ! -f "${SOURCE}" ]; then
    echo "  SKIP: ${example}.swift not found"
    continue
  fi

  if "${TINYSWIFT}" compile --phase=check "${SOURCE}" 2>"${TMPDIR}/stderr_${example}.txt"; then
    echo "  PASS: ${example}"
    PASS=$((PASS + 1))
  else
    echo "  FAIL: ${example}"
    FAIL=$((FAIL + 1))
    ERRORS="${ERRORS}\n  - ${example} (check phase failed)"
    if [ -s "${TMPDIR}/stderr_${example}.txt" ]; then
      echo "    Error output:"
      sed 's/^/    /' "${TMPDIR}/stderr_${example}.txt"
    fi
  fi
done
echo ""

# Phase 2: Full-pipeline verification for executable examples.
echo "--- Phase 2: Executable Examples (full pipeline) ---"
for example in "${EXECUTABLE_EXAMPLES[@]}"; do
  SOURCE="${EXAMPLES_DIR}/${example}.swift"
  if [ ! -f "${SOURCE}" ]; then
    echo "  SKIP: ${example}.swift not found"
    continue
  fi

  # Step 2a: Compile through codegen.
  if "${TINYSWIFT}" compile --emit-object \
       --output="${TMPDIR}/${example}.o" "${SOURCE}" \
       2>"${TMPDIR}/stderr_${example}.txt"; then
    echo "  PASS: ${example} (codegen)"
    PASS=$((PASS + 1))
  else
    echo "  FAIL: ${example} (codegen)"
    FAIL=$((FAIL + 1))
    ERRORS="${ERRORS}\n  - ${example} (codegen failed)"
    if [ -s "${TMPDIR}/stderr_${example}.txt" ]; then
      echo "    Error output:"
      sed 's/^/    /' "${TMPDIR}/stderr_${example}.txt"
    fi
    continue
  fi

  # Step 2b: Compile and link to executable.
  if "${TINYSWIFT}" compile \
       --output="${TMPDIR}/${example}" "${SOURCE}" \
       2>"${TMPDIR}/stderr_link_${example}.txt"; then
    echo "  PASS: ${example} (link)"
    PASS=$((PASS + 1))

    # Step 2c: Execute the binary.
    if "${TMPDIR}/${example}" >"${TMPDIR}/stdout_${example}.txt" \
         2>"${TMPDIR}/stderr_run_${example}.txt"; then
      echo "  PASS: ${example} (run)"
      PASS=$((PASS + 1))
    else
      EXIT_CODE=$?
      echo "  FAIL: ${example} (run - exit code ${EXIT_CODE})"
      FAIL=$((FAIL + 1))
      ERRORS="${ERRORS}\n  - ${example} (runtime exit code ${EXIT_CODE})"
    fi
  else
    echo "  FAIL: ${example} (link)"
    FAIL=$((FAIL + 1))
    ERRORS="${ERRORS}\n  - ${example} (link failed)"
    if [ -s "${TMPDIR}/stderr_link_${example}.txt" ]; then
      echo "    Error output:"
      sed 's/^/    /' "${TMPDIR}/stderr_link_${example}.txt"
    fi
  fi
done
echo ""

# Summary.
TOTAL=$((PASS + FAIL))
echo "========================================"
echo "Results: ${PASS}/${TOTAL} passed, ${FAIL} failed"
echo "========================================"

if [ ${FAIL} -gt 0 ]; then
  echo ""
  echo "Failures:"
  echo -e "${ERRORS}"
  exit 1
fi

echo ""
echo "All examples verified successfully!"
exit 0
