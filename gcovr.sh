#!/usr/bin/env bash
# Coverage report for Freyr product code.
# Configure with: cmake -B build -DCMAKE_BUILD_TYPE=Debug -DFREYR_COVERAGE=ON -DFREYR_PROFILING=OFF
# then rebuild, run tests, and invoke this script from the repo root.
# Outputs: nested HTML, Cobertura XML, and LCOV under ./coverage-report/ (see gcovr.cfg).

set -euo pipefail

mkdir -p ./coverage-report

gcovr
