$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $repo
cmake -S . -B build -G 'Visual Studio 17 2022' -A x64 -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
cmake --build build --config Release --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Release build failed' }
ctest --test-dir build -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Core tests failed' }
& (Join-Path $PSScriptRoot 'Package-Windows.ps1')
