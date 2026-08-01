#!/usr/bin/env bash
# Coverage report for Freyr product code.
# Configure with: cmake -B build -DCMAKE_BUILD_TYPE=Debug -DFREYR_COVERAGE=ON -DFREYR_PROFILING=OFF
# then rebuild and invoke this script from the repo root.
# Outputs: nested HTML, Cobertura XML, and LCOV under ./coverage-report/ (see gcovr.cfg).
#
# Usage:
#   ./gcovr.sh          Clean counters, run tests, generate reports
#   ./gcovr.sh clean    Delete .gcda counters and coverage-report/ only
#
# Override the build directory with BUILD_DIR=path when multiple coverage builds exist.

set -euo pipefail

clean_coverage() {
  find . -type f -name '*.gcda' \
    -not -path './.git/*' \
    -not -path './vendor/*' \
    -delete
  rm -rf ./coverage-report
}

resolve_build_dir() {
  if [[ -n "${BUILD_DIR:-}" ]]; then
    if [[ ! -f "${BUILD_DIR}/CMakeCache.txt" ]]; then
      echo "BUILD_DIR=${BUILD_DIR} has no CMakeCache.txt" >&2
      exit 1
    fi
    printf '%s\n' "${BUILD_DIR}"
    return
  fi

  local -a dirs=()
  local cache dir
  while IFS= read -r cache; do
    dir=$(dirname "$cache")
    if grep -q 'FREYR_COVERAGE:BOOL=ON' "$cache"; then
      dirs+=("$dir")
    fi
  done < <(find . -name CMakeCache.txt \
    -not -path './.git/*' \
    -not -path './vendor/*' \
    -not -path '*/_deps/*')

  if [[ ${#dirs[@]} -eq 0 ]]; then
    echo "No coverage-enabled build dir found (FREYR_COVERAGE=ON). Configure one or set BUILD_DIR=." >&2
    exit 1
  fi

  if [[ ${#dirs[@]} -gt 1 ]]; then
    echo "Multiple coverage build dirs found:" >&2
    printf '  %s\n' "${dirs[@]}" >&2
    echo "Set BUILD_DIR to the preferred one." >&2
    exit 1
  fi

  printf '%s\n' "${dirs[0]}"
}

run_tests() {
  local build_dir=$1
  echo "Running tests in ${build_dir}"
  ctest --test-dir "${build_dir}" --output-on-failure --build-config Debug
}

generate_report() {
  mkdir -p ./coverage-report
  gcovr
}

case "${1:-}" in
  clean)
    clean_coverage
    ;;
  "")
    clean_coverage
    run_tests "$(resolve_build_dir)"
    generate_report
    ;;
  *)
    echo "Usage: $0 [clean]" >&2
    exit 1
    ;;
esac
