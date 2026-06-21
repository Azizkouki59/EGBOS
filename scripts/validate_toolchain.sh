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

check_version "gcc" "13.1" "$(gcc -dumpfullversion)"
check_version "clang" "17.0" "$(clang --version | head -1 | grep -oP '\d+\.\d+' | head -1)"
check_version "cmake" "3.27" "$(cmake --version | head -1 | grep -oP '\d+\.\d+')"
check_version "flatc" "24.3" "$(flatc --version | grep -oP '\d+\.\d+')"

python3 --version >/dev/null 2>&1 || { echo "FAIL: python3 not found"; ERRORS=$((ERRORS+1)); }
uv --version >/dev/null 2>&1     || { echo "FAIL: uv not found"; ERRORS=$((ERRORS+1)); }
rustc --version >/dev/null 2>&1   || { echo "FAIL: rustc not found"; ERRORS=$((ERRORS+1)); }
cargo --version >/dev/null 2>&1   || { echo "FAIL: cargo not found"; ERRORS=$((ERRORS+1)); }

if [ "$ERRORS" -gt 0 ]; then
  echo "TOOLCHAIN VALIDATION FAILED: $ERRORS error(s)"
  exit 1
fi
echo "TOOLCHAIN VALIDATION PASSED"
