#!/bin/bash
# Part of the TinySwift compiler project, under the Apache License v2.0 with LLVM
# Exceptions. See /LICENSE for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Verifies a single TinySwift example compiles (and optionally runs) correctly.
#
# Usage: verify_example.sh <tinyswift_binary> <source_file> <mode>
#   mode: "check"   - compile through semantic analysis only
#         "codegen"  - compile through code generation (produces .o)
#         "run"      - compile, link, and execute

set -euo pipefail

TINYSWIFT="$1"
SOURCE="$2"
MODE="$3"

BASENAME="$(basename "${SOURCE}" .swift)"

echo "=== Verifying ${BASENAME} (mode: ${MODE}) ==="

case "${MODE}" in
  check)
    echo "  Compiling through check phase..."
    "${TINYSWIFT}" compile --phase=check "${SOURCE}"
    echo "  PASS: ${BASENAME} - check phase completed successfully"
    ;;

  codegen)
    echo "  Compiling through codegen phase..."
    TMPDIR="$(mktemp -d)"
    trap 'rm -rf "${TMPDIR}"' EXIT
    "${TINYSWIFT}" compile --emit-object --output="${TMPDIR}/${BASENAME}.o" "${SOURCE}"
    if [ -f "${TMPDIR}/${BASENAME}.o" ]; then
      echo "  PASS: ${BASENAME} - codegen produced object file"
    else
      echo "  FAIL: ${BASENAME} - no object file produced"
      exit 1
    fi
    ;;

  run)
    echo "  Compiling and linking..."
    TMPDIR="$(mktemp -d)"
    trap 'rm -rf "${TMPDIR}"' EXIT
    "${TINYSWIFT}" compile --output="${TMPDIR}/${BASENAME}" "${SOURCE}"
    if [ ! -f "${TMPDIR}/${BASENAME}" ]; then
      echo "  FAIL: ${BASENAME} - no executable produced"
      exit 1
    fi
    echo "  Running ${BASENAME}..."
    EXIT_CODE=0
    "${TMPDIR}/${BASENAME}" || EXIT_CODE=$?
    if [ ${EXIT_CODE} -eq 0 ]; then
      echo "  PASS: ${BASENAME} - executed successfully (exit code 0)"
    else
      echo "  FAIL: ${BASENAME} - exited with code ${EXIT_CODE}"
      exit 1
    fi
    ;;

  *)
    echo "Unknown mode: ${MODE}"
    exit 1
    ;;
esac
