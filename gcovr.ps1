# Coverage report for Freyr product code.
# Configure with: cmake -B build -DCMAKE_BUILD_TYPE=Debug -DFREYR_COVERAGE=ON -DFREYR_PROFILING=OFF
# then rebuild and invoke this script from the repo root.
# Outputs: nested HTML, Cobertura XML, and LCOV under ./coverage-report/ (see gcovr.cfg).
#
# Usage:
#   ./gcovr.ps1          Clean counters, run tests, generate reports
#   ./gcovr.ps1 clean    Delete .gcda counters and coverage-report/ only
#
# Override the build directory with $env:BUILD_DIR when multiple coverage builds exist.

$ErrorActionPreference = 'Stop'

function Clear-CoverageData {
    Get-ChildItem -Path . -Filter *.gcda -File -Recurse -ErrorAction SilentlyContinue |
        Where-Object {
            $_.FullName -notmatch '[\\/]\.git[\\/]' -and
            $_.FullName -notmatch '[\\/]vendor[\\/]'
        } |
        Remove-Item -Force

    if (Test-Path ./coverage-report) {
        Remove-Item -Recurse -Force ./coverage-report
    }
}

function Resolve-BuildDir {
    if ($env:BUILD_DIR) {
        $cache = Join-Path $env:BUILD_DIR 'CMakeCache.txt'
        if (-not (Test-Path $cache)) {
            throw "BUILD_DIR=$($env:BUILD_DIR) has no CMakeCache.txt"
        }
        return $env:BUILD_DIR
    }

    $dirs = @(
        Get-ChildItem -Path . -Filter CMakeCache.txt -File -Recurse -ErrorAction SilentlyContinue |
            Where-Object {
                $_.FullName -notmatch '[\\/]\.git[\\/]' -and
                $_.FullName -notmatch '[\\/]vendor[\\/]' -and
                $_.FullName -notmatch '[\\/]_deps[\\/]'
            } |
            Where-Object { Select-String -Path $_.FullName -Pattern 'FREYR_COVERAGE:BOOL=ON' -Quiet } |
            ForEach-Object { $_.DirectoryName }
    )

    if ($dirs.Count -eq 0) {
        throw 'No coverage-enabled build dir found (FREYR_COVERAGE=ON). Configure one or set BUILD_DIR.'
    }

    if ($dirs.Count -gt 1) {
        throw ("Multiple coverage build dirs found:`n  {0}`nSet BUILD_DIR to the preferred one." -f ($dirs -join "`n  "))
    }

    return $dirs[0]
}

function Invoke-CoverageTests([string]$BuildDir) {
    Write-Host "Running tests in $BuildDir"
    & ctest --test-dir $BuildDir --output-on-failure --build-config Debug
    if ($LASTEXITCODE -ne 0) {
        throw "ctest failed with exit code $LASTEXITCODE"
    }
}

function New-CoverageReport {
    New-Item -ItemType Directory -Force -Path ./coverage-report | Out-Null
    & gcovr
    if ($LASTEXITCODE -ne 0) {
        throw "gcovr failed with exit code $LASTEXITCODE"
    }
}

$command = if ($args.Count -gt 0) { $args[0] } else { '' }

switch ($command) {
    'clean' {
        Clear-CoverageData
    }
    '' {
        Clear-CoverageData
        Invoke-CoverageTests (Resolve-BuildDir)
        New-CoverageReport
    }
    default {
        Write-Error "Usage: ./gcovr.ps1 [clean]"
        exit 1
    }
}
