$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build = Join-Path $repo 'build'
$dist = Join-Path $repo 'out/SkellCast-Windows-x64'
New-Item -ItemType Directory -Force -Path $dist | Out-Null
$plugin = Get-ChildItem -Path $build -Directory -Recurse | Where-Object { $_.Name -eq 'SkellCast.vst3' } | Select-Object -First 1
if (-not $plugin) { throw 'Windows VST3 bundle not found. Run the Release build first.' }
Copy-Item -Path $plugin.FullName -Destination $dist -Recurse -Force
$app = Get-ChildItem -Path $build -File -Recurse -Filter 'SkellCastReceiver.exe' | Select-Object -First 1
if (-not $app) { throw 'SkellCastReceiver.exe not found. Run the Release build first.' }
Copy-Item -Path $app.FullName -Destination $dist -Force
Copy-Item -Path (Join-Path $repo 'README.md') -Destination $dist -Force
Copy-Item -Path (Join-Path $repo 'TESTING.md') -Destination $dist -Force
Write-Host "Packaged: $dist"
