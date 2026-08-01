# Coverage report for Freyr product code.
# Configure with: cmake -B build -DFREYR_COVERAGE=ON -DFREYR_PROFILING=OFF
# then rebuild, run tests, and invoke this script from the repo root.

$ErrorActionPreference = 'Stop'

New-Item -ItemType Directory -Force -Path ./coverage-report | Out-Null

gcovr --html-details ./coverage-report/coverage.html `
  --gcov-ignore-parse-errors `
  --filter src/ `
  --filter include/ `
  --exclude '.*MPMCQueue\.hpp$' `
  --exclude '.*Processor\.hpp$' `
  --exclude '.*Profiling\.hpp$' `
  --exclude '.*perfetto.*'
