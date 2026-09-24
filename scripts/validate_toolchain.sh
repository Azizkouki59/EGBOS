#!/usr/bin/env bash
set -euo pipefail

ERRORS=0

check_version() {
  local tool="$1" min="$2" actual="$3"
  if [ "$(printf '%s\n' "$min" "$actual" | sort -V | head -n1)" != "$min" ]; then
    echo "FAIL: $tool version $actual < required $min"
    ERRORS=$((ERRORS + 1))
  else
    echo "OK:   $tool version $actual >= $min"
  fi
}

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROJECT_FLATC="${EGBOS_FLATC:-${REPO_ROOT}/build_native/_deps/flatbuffers-build/flatc}"

check_version "gcc" "13.1" "$(gcc -dumpfullversion)"
check_version "clang" "17.0" "$(clang --version | head -1 | grep -oP '\d+\.\d+' | head -1)"
check_version "cmake" "3.27" "$(cmake --version | head -1 | grep -oP '\d+\.\d+')"

if [ -x "${PROJECT_FLATC}" ]; then
  FLATC_BIN="${PROJECT_FLATC}"
  echo "INFO: using project-managed flatc: ${FLATC_BIN}"
else
  FLATC_BIN="$(command -v flatc || true)"
fi

if [ -z "${FLATC_BIN}" ]; then
  echo "FAIL: flatc not found"
  ERRORS=$((ERRORS + 1))
else
  check_version "flatc" "24.3" "$("${FLATC_BIN}" --version | grep -oP '\d+\.\d+')"
fi

python3 --version >/dev/null 2>&1 || { echo "FAIL: python3 not found"; ERRORS=$((ERRORS+1)); }
uv --version >/dev/null 2>&1     || { echo "FAIL: uv not found"; ERRORS=$((ERRORS+1)); }
rustc --version >/dev/null 2>&1   || { echo "FAIL: rustc not found"; ERRORS=$((ERRORS+1)); }
cargo --version >/dev/null 2>&1   || { echo "FAIL: cargo not found"; ERRORS=$((ERRORS+1)); }

if [ "$ERRORS" -gt 0 ]; then
  echo "TOOLCHAIN VALIDATION FAILED: $ERRORS error(s)"
  exit 1
fi
echo "TOOLCHAIN VALIDATION PASSED"
