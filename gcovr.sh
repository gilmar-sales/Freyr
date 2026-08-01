#!/usr/bin/env bash
# Coverage report for Freyr product code.
# Prefer measuring with: cmake -B build -DFREYR_PROFILING=OFF
# then rebuild, run tests, and invoke this script from the repo root.

set -euo pipefail

mkdir -p ./coverage-report

gcovr --html-details ./coverage-report/coverage.html \
  --gcov-ignore-parse-errors \
  --filter src/ \
  --filter include/ \
  --exclude '.*MPMCQueue\.hpp$' \
  --exclude '.*Processor\.hpp$' \
  --exclude '.*Profiling\.hpp$' \
  --exclude '.*perfetto.*'
