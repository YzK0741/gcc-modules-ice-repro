$ErrorActionPreference = 'Stop'

# Reproducer for the GCC 15.2.0 ICE (see README.md).
# Usage:  .\repro.ps1 [-Cxx "C:\path\to\c++.exe"]
param(
    [string]$Cxx = 'c++'
)

$ErrorActionPreference = 'Continue'

Write-Host "compiler: $Cxx"
& $Cxx --version | Select-Object -First 1

Push-Location (Join-Path $PSScriptRoot 'minimal')
Remove-Item -Recurse -Force gcm.cache, *.o -ErrorAction SilentlyContinue

Write-Host "`n[1/2] compiling partition log.cppm"
& $Cxx -std=c++23 -fmodules-ts -c log.cppm -o log.o
Write-Host "exit=$LASTEXITCODE (expected 0)"

Write-Host "`n[2/2] compiling primary interface unit utility.cppm"
& $Cxx -std=c++23 -fmodules-ts -c utility.cppm -o utility.o
Write-Host "exit=$LASTEXITCODE (expected: segfault / ICE)"

Pop-Location
