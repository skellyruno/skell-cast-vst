$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build = Join-Path $repo 'build'
$outRoot = Join-Path $repo 'out'
$dist = Join-Path $outRoot 'SkellCast-VST3-Windows-x64'
$stage = Join-Path $outRoot '.SkellCast-VST3-Windows-x64-staging'
$verifier = Join-Path $repo 'tools/verify_vst3_package.py'

New-Item -ItemType Directory -Force -Path $outRoot | Out-Null
if (Test-Path -LiteralPath $stage) {
    Remove-Item -LiteralPath $stage -Recurse -Force
}

# Validate into a fresh staging directory first. A failed package must not
# delete the last known-good local preview package.
& python $verifier --source-dir $build --package-dir $stage --repo-root $repo
if ($LASTEXITCODE -ne 0) { throw 'SkellCast VST3 package validation failed' }

$previous = $null
if (Test-Path -LiteralPath $dist) {
    $backupRoot = Join-Path $outRoot 'previous'
    New-Item -ItemType Directory -Force -Path $backupRoot | Out-Null
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    $previous = Join-Path $backupRoot "SkellCast-VST3-Windows-x64-$stamp"
    Move-Item -LiteralPath $dist -Destination $previous
}

try {
    Move-Item -LiteralPath $stage -Destination $dist
}
catch {
    if ($previous -and (Test-Path -LiteralPath $previous) -and -not (Test-Path -LiteralPath $dist)) {
        Move-Item -LiteralPath $previous -Destination $dist
    }
    throw
}

Write-Host "Packaged verified Windows x64 VST3 preview: $dist"
if ($previous) { Write-Host "Previous package preserved at: $previous" }
