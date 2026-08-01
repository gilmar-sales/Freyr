#!/usr/bin/env bash
# Coverage report for Freyr product code.
# Configure with: cmake -B build -DCMAKE_BUILD_TYPE=Debug -DFREYR_COVERAGE=ON -DFREYR_PROFILING=OFF
# then rebuild, run tests, and invoke this script from the repo root.

set -euo pipefail

mkdir -p ./coverage-report

gcovr --html-details ./coverage-report/coverage.html \
  --gcov-ignore-parse-errors \
  --exclude-throw-branches \
  --exclude-unreachable-branches \
  --filter src/ \
  --filter include/ \
  --exclude '.*MPMCQueue\.hpp$' \
  --exclude '.*Processor\.hpp$' \
  --exclude '.*Profiling\.hpp$' \
  --exclude '.*perfetto.*'
