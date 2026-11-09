$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build = Join-Path $repo 'build'
$dist = Join-Path $repo 'out/SkellCast-VST3-Windows-x64'
if (Test-Path $dist) { Remove-Item -Path $dist -Recurse -Force }
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$bundles = @(Get-ChildItem -Path $build -Directory -Recurse -ErrorAction Stop |
    Where-Object { $_.Name -eq 'SkellCast.vst3' -and $_.FullName -match '(\\|/)Release(\\|/)' })
$bundle = $bundles | Where-Object {
    Test-Path (Join-Path $_.FullName 'Contents/x86_64-win/SkellCast.vst3')
} | Select-Object -First 1
if (-not $bundle) {
    throw 'Could not find a validated Release SkellCast.vst3 Windows bundle containing Contents/x86_64-win/SkellCast.vst3'
}
Copy-Item -Path $bundle.FullName -Destination $dist -Recurse -Force
$binary = Join-Path $dist 'SkellCast.vst3/Contents/x86_64-win/SkellCast.vst3'
if (-not (Test-Path $binary) -or (Get-Item $binary).Length -lt 10000) {
    throw 'VST3 Windows binary is missing or unexpectedly small.'
}
Copy-Item (Join-Path $repo 'README.md') $dist
Copy-Item (Join-Path $repo 'TESTING.md') $dist
Write-Host "Packaged only the VST3: $dist"
