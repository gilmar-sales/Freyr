# Coverage report for Freyr product code.
# Configure with: cmake -B build -DCMAKE_BUILD_TYPE=Debug -DFREYR_COVERAGE=ON -DFREYR_PROFILING=OFF
# then rebuild, run tests, and invoke this script from the repo root.
# Outputs: nested HTML, Cobertura XML, and LCOV under ./coverage-report/ (see gcovr.cfg).

$ErrorActionPreference = 'Stop'

New-Item -ItemType Directory -Force -Path ./coverage-report | Out-Null

gcovr
