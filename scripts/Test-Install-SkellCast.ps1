[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][ValidateNotNullOrEmpty()][string]$PackageRoot,
    [Parameter(Mandatory = $true)][ValidateNotNullOrEmpty()][string]$InstallerPath,
    [Parameter(Mandatory = $true)][ValidateNotNullOrEmpty()][string]$TestRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:assertions = 0
$script:passed = $false

function Assert([bool]$Condition, [string]$Message) {
    $script:assertions++
    if (-not $Condition) { throw $Message }
}
function Get-Hash([string]$Path) {
    (Get-FileHash -LiteralPath $Path -Algorithm SHA256 -ErrorAction Stop).Hash.ToLowerInvariant()
}
function Write-PackageChecksums([string]$Root) {
    $rootFull = [System.IO.Path]::GetFullPath($Root)
    $sumPath = Join-Path $rootFull 'SHA256SUMS.txt'
    $files = @(Get-ChildItem -LiteralPath $rootFull -Recurse -File -Force |
        Where-Object { -not [string]::Equals($_.FullName, $sumPath, [System.StringComparison]::OrdinalIgnoreCase) } |
        Sort-Object FullName)
    $lines = @()
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($rootFull.Length).TrimStart([char[]]@('\', '/')).Replace('\', '/')
        $lines += ((Get-Hash $file.FullName) + '  ' + $relative)
    }
    [System.IO.File]::WriteAllLines($sumPath, [string[]]$lines, [System.Text.Encoding]::ASCII)
}
function Invoke-ExpectedFailure([scriptblock]$Action, [string]$Message) {
    $failed = $false
    try { & $Action } catch { $failed = $true }
    Assert $failed $Message
}
function Copy-Package([string]$Source, [string]$Destination) {
    Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -ErrorAction Stop
}
function New-TestJunction([string]$LinkPath, [string]$TargetPath) {
    New-Item -ItemType Directory -Path $TargetPath -Force | Out-Null
    New-Item -ItemType Junction -Path $LinkPath -Target $TargetPath -ErrorAction Stop | Out-Null
    $link = Get-Item -LiteralPath $LinkPath -Force
    Assert (($link.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0) "Test junction was not created: $LinkPath"
}
function Remove-TestJunction([string]$LinkPath) {
    Remove-Item -LiteralPath $LinkPath -Force -ErrorAction Stop
}
function Set-PackageVersion([string]$Root, [string]$Version) {
    $moduleInfoPath = Join-Path $Root 'SkellCast.vst3\Contents\Resources\moduleinfo.json'
    $moduleInfo = Get-Content -LiteralPath $moduleInfoPath -Raw | ConvertFrom-Json
    $moduleInfo.Version = $Version
    foreach ($class in $moduleInfo.Classes) { $class.Version = $Version }
    [System.IO.File]::WriteAllText($moduleInfoPath, (($moduleInfo | ConvertTo-Json -Depth 20) + [Environment]::NewLine), [System.Text.UTF8Encoding]::new($false))

    $buildInfoPath = Join-Path $Root 'build-info.json'
    $buildInfo = Get-Content -LiteralPath $buildInfoPath -Raw | ConvertFrom-Json
    $buildInfo.version = $Version
    $buildInfo.plugin.version = $Version
    [System.IO.File]::WriteAllText($buildInfoPath, (($buildInfo | ConvertTo-Json -Depth 20) + [Environment]::NewLine), [System.Text.UTF8Encoding]::new($false))
    Write-PackageChecksums $Root
}

$package = [System.IO.Path]::GetFullPath($PackageRoot)
$installer = [System.IO.Path]::GetFullPath($InstallerPath)
$testRoot = [System.IO.Path]::GetFullPath($TestRoot)
if (-not (Test-Path -LiteralPath $package -PathType Container)) { throw "PackageRoot not found: $package" }
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) { throw "Installer script not found: $installer" }
if (Test-Path -LiteralPath $testRoot) { throw "Refusing to use an existing test root: $testRoot" }
$pathRoot = [System.IO.Path]::GetPathRoot($testRoot)
if ([string]::Equals($testRoot.TrimEnd([char[]]@('\', '/')), $pathRoot.TrimEnd([char[]]@('\', '/')), [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Refusing a filesystem root as the test root.'
}
$parent = [System.IO.Path]::GetDirectoryName($testRoot)
if (-not (Test-Path -LiteralPath $parent -PathType Container)) { New-Item -ItemType Directory -Path $parent -Force | Out-Null }
New-Item -ItemType Directory -Path $testRoot -ErrorAction Stop | Out-Null
$token = [Guid]::NewGuid().ToString('N')
$marker = Join-Path $testRoot ('.skellcast-test-root-' + $token)
[System.IO.File]::WriteAllText($marker, $token, [System.Text.UTF8Encoding]::new($false))

try {
    $packageV1 = Join-Path $testRoot 'package-v1'
    Copy-Package $package $packageV1

    $target = Join-Path $testRoot 'VST3'
    $backups = Join-Path $testRoot 'Backups'
    $whatIf = & $installer -PackageRoot $packageV1 -Vst3Directory $target -BackupRoot $backups -WhatIf
    Assert ($whatIf.status -eq 'what-if') 'WhatIf did not return the expected status.'
    Assert (-not (Test-Path -LiteralPath $target)) 'WhatIf created the installation target.'
    Assert (-not (Test-Path -LiteralPath $backups)) 'WhatIf created a backup directory.'

    $fresh = & $installer -PackageRoot $packageV1 -Vst3Directory $target -BackupRoot $backups -Confirm:$false
    Assert ($fresh.status -eq 'installed') 'Fresh install did not complete.'
    Assert (Test-Path -LiteralPath (Join-Path $target 'SkellCast.vst3\Contents\x86_64-win\SkellCast.vst3') -PathType Leaf) 'Installed VST3 module is missing.'
    Assert ([string]$fresh.version -eq '0.8.0') 'Fresh install has an unexpected version.'

    $again = & $installer -PackageRoot $packageV1 -Vst3Directory $target -BackupRoot $backups -Confirm:$false
    Assert ($again.status -eq 'already-installed') 'Repeated install was not idempotent.'
    Assert (-not (Test-Path -LiteralPath $backups)) 'Idempotent install unexpectedly created a backup.'

    $packageV2 = Join-Path $testRoot 'package-v2'
    Copy-Package $packageV1 $packageV2
    Set-PackageVersion $packageV2 '0.8.1'
    $updated = & $installer -PackageRoot $packageV2 -Vst3Directory $target -BackupRoot $backups -Confirm:$false
    Assert ($updated.status -eq 'installed') 'Update did not complete.'
    Assert ([string]$updated.version -eq '0.8.1') 'Updated version is incorrect.'
    Assert (-not [string]::IsNullOrWhiteSpace([string]$updated.backup_path)) 'Update did not create a persistent backup.'
    $originalBackup = [string]$updated.backup_path
    $backupManifest = Get-Content -LiteralPath (Join-Path $originalBackup 'backup-manifest.json') -Raw | ConvertFrom-Json
    Assert ($backupManifest.version -eq '0.8.0') 'Pre-update backup does not contain the original version.'
    Assert (Test-Path -LiteralPath (Join-Path $originalBackup 'SkellCast.vst3') -PathType Container) 'Pre-update VST3 backup is missing.'

    $rolledBack = & $installer -RollbackFrom $originalBackup -Vst3Directory $target -BackupRoot $backups -Confirm:$false
    Assert ($rolledBack.status -eq 'rolled-back') 'Rollback did not complete.'
    Assert ([string]$rolledBack.version -eq '0.8.0') 'Rollback did not restore the original version.'
    Assert (-not [string]::IsNullOrWhiteSpace([string]$rolledBack.backup_path)) 'Rollback did not preserve the version it replaced.'

    $badBinaryPackage = Join-Path $testRoot 'bad-binary-package'
    Copy-Package $packageV1 $badBinaryPackage
    $moduleBinary = Join-Path $badBinaryPackage 'SkellCast.vst3\Contents\x86_64-win\SkellCast.vst3'
    [System.IO.File]::AppendAllText($moduleBinary, 'tamper')
    $badBinaryTarget = Join-Path $testRoot 'BadBinaryTarget'
    Invoke-ExpectedFailure {
        & $installer -PackageRoot $badBinaryPackage -Vst3Directory $badBinaryTarget -BackupRoot (Join-Path $testRoot 'BadBinaryBackups') -Confirm:$false
    } 'Installer accepted a package with a tampered module.'
    Assert (-not (Test-Path -LiteralPath $badBinaryTarget)) 'Tampered package created an installation target.'

    $badPathPackage = Join-Path $testRoot 'bad-path-package'
    Copy-Package $packageV1 $badPathPackage
    Add-Content -LiteralPath (Join-Path $badPathPackage 'SHA256SUMS.txt') -Value (('0' * 64) + '  ../escape.txt')
    $badPathTarget = Join-Path $testRoot 'BadPathTarget'
    Invoke-ExpectedFailure {
        & $installer -PackageRoot $badPathPackage -Vst3Directory $badPathTarget -BackupRoot (Join-Path $testRoot 'BadPathBackups') -Confirm:$false
    } 'Installer accepted a checksum path traversal.'
    Assert (-not (Test-Path -LiteralPath $badPathTarget)) 'Path traversal package created an installation target.'

    $duplicateJsonPackage = Join-Path $testRoot 'duplicate-json-package'
    Copy-Package $packageV1 $duplicateJsonPackage
    [System.IO.File]::WriteAllText((Join-Path $duplicateJsonPackage 'build-info.json'),
        '{"product":"SkellCast","product":"Other"}', [System.Text.UTF8Encoding]::new($false))
    Write-PackageChecksums $duplicateJsonPackage
    $duplicateJsonTarget = Join-Path $testRoot 'DuplicateJsonTarget'
    Invoke-ExpectedFailure {
        & $installer -PackageRoot $duplicateJsonPackage -Vst3Directory $duplicateJsonTarget -BackupRoot (Join-Path $testRoot 'DuplicateJsonBackups') -Confirm:$false
    } 'Installer accepted duplicate JSON properties.'
    Assert (-not (Test-Path -LiteralPath $duplicateJsonTarget)) 'Duplicate JSON package created an installation target.'

    $nonFinitePackage = Join-Path $testRoot 'non-finite-package'
    Copy-Package $packageV1 $nonFinitePackage
    $nonFinitePath = Join-Path $nonFinitePackage 'build-info.json'
    $nonFiniteRaw = (Get-Content -LiteralPath $nonFinitePath -Raw).TrimEnd()
    if (-not $nonFiniteRaw.EndsWith('}')) { throw 'Test fixture build-info.json is not a JSON object.' }
    $nonFiniteRaw = $nonFiniteRaw.Substring(0, $nonFiniteRaw.Length - 1) + ',"test_number":NaN}' + [Environment]::NewLine
    [System.IO.File]::WriteAllText($nonFinitePath, $nonFiniteRaw, [System.Text.UTF8Encoding]::new($false))
    Write-PackageChecksums $nonFinitePackage
    $nonFiniteTarget = Join-Path $testRoot 'NonFiniteTarget'
    Invoke-ExpectedFailure {
        & $installer -PackageRoot $nonFinitePackage -Vst3Directory $nonFiniteTarget -BackupRoot (Join-Path $testRoot 'NonFiniteBackups') -Confirm:$false
    } 'Installer accepted a non-finite JSON number.'
    Assert (-not (Test-Path -LiteralPath $nonFiniteTarget)) 'Non-finite JSON package created an installation target.'

    $badCategoryPackage = Join-Path $testRoot 'bad-category-package'
    Copy-Package $packageV1 $badCategoryPackage
    $categoryPath = Join-Path $badCategoryPackage 'SkellCast.vst3\Contents\Resources\moduleinfo.json'
    $categoryJson = Get-Content -LiteralPath $categoryPath -Raw | ConvertFrom-Json
    $categoryJson.Classes[0].Category = @('Audio Module Class')
    [System.IO.File]::WriteAllText($categoryPath, (($categoryJson | ConvertTo-Json -Depth 20) + [Environment]::NewLine), [System.Text.UTF8Encoding]::new($false))
    Write-PackageChecksums $badCategoryPackage
    $badCategoryTarget = Join-Path $testRoot 'BadCategoryTarget'
    Invoke-ExpectedFailure {
        & $installer -PackageRoot $badCategoryPackage -Vst3Directory $badCategoryTarget -BackupRoot (Join-Path $testRoot 'BadCategoryBackups') -Confirm:$false
    } 'Installer accepted a non-string VST3 class Category.'
    Assert (-not (Test-Path -LiteralPath $badCategoryTarget)) 'Invalid moduleinfo created an installation target.'

    $badBackup = Join-Path $testRoot 'bad-backup'
    Copy-Package $originalBackup $badBackup
    $badBackupModule = Join-Path $badBackup 'SkellCast.vst3\Contents\x86_64-win\SkellCast.vst3'
    [System.IO.File]::AppendAllText($badBackupModule, 'tamper')
    $badRollbackTarget = Join-Path $testRoot 'BadRollbackTarget'
    Invoke-ExpectedFailure {
        & $installer -RollbackFrom $badBackup -Vst3Directory $badRollbackTarget -BackupRoot (Join-Path $testRoot 'BadRollbackBackups') -Confirm:$false
    } 'Rollback accepted a tampered backup.'
    Assert (-not (Test-Path -LiteralPath $badRollbackTarget)) 'Tampered backup created a rollback target.'

    $invalidTargetRoot = Join-Path $testRoot 'InvalidExistingTarget'
    New-Item -ItemType Directory -Path $invalidTargetRoot | Out-Null
    $invalidTarget = Join-Path $invalidTargetRoot 'SkellCast.vst3'
    [System.IO.File]::WriteAllText($invalidTarget, 'preserve existing data')
    Invoke-ExpectedFailure {
        & $installer -PackageRoot $packageV1 -Vst3Directory $invalidTargetRoot -BackupRoot (Join-Path $testRoot 'InvalidTargetBackups') -Confirm:$false
    } 'Installer overwrote an invalid pre-existing target.'
    Assert ((Get-Content -LiteralPath $invalidTarget -Raw) -eq 'preserve existing data') 'Installer modified an invalid pre-existing target.'

    # A junction anywhere inside a package must fail before the installer writes a destination.
    $reparsePackage = Join-Path $testRoot 'reparse-package'
    Copy-Package $packageV1 $reparsePackage
    $packageJunction = Join-Path $reparsePackage 'external'
    New-TestJunction $packageJunction (Join-Path $testRoot 'outside-package')
    $reparsePackageTarget = Join-Path $testRoot 'ReparsePackageTarget'
    Invoke-ExpectedFailure {
        & $installer -PackageRoot $reparsePackage -Vst3Directory $reparsePackageTarget -BackupRoot (Join-Path $testRoot 'ReparsePackageBackups') -Confirm:$false
    } 'Installer accepted a package tree containing a junction.'
    Assert (-not (Test-Path -LiteralPath $reparsePackageTarget)) 'Reparse-point package created an installation target.'
    Remove-TestJunction $packageJunction

    # Rollback must reject a backup tree containing a junction before touching the destination.
    $reparseBackup = Join-Path $testRoot 'reparse-backup'
    Copy-Package $originalBackup $reparseBackup
    $backupJunction = Join-Path $reparseBackup 'external'
    New-TestJunction $backupJunction (Join-Path $testRoot 'outside-backup')
    $reparseBackupTarget = Join-Path $testRoot 'ReparseBackupTarget'
    Invoke-ExpectedFailure {
        & $installer -RollbackFrom $reparseBackup -Vst3Directory $reparseBackupTarget -BackupRoot (Join-Path $testRoot 'ReparseRollbackBackups') -Confirm:$false
    } 'Rollback accepted a backup tree containing a junction.'
    Assert (-not (Test-Path -LiteralPath $reparseBackupTarget)) 'Reparse-point backup created a rollback target.'
    Remove-TestJunction $backupJunction

    # A redirected VST3 destination root must not receive an installation.
    $outsideVst3 = Join-Path $testRoot 'outside-vst3'
    New-Item -ItemType Directory -Path $outsideVst3 -Force | Out-Null
    $redirectedVst3 = Join-Path $testRoot 'redirected-vst3'
    New-TestJunction $redirectedVst3 $outsideVst3
    Invoke-ExpectedFailure {
        & $installer -PackageRoot $packageV1 -Vst3Directory $redirectedVst3 -BackupRoot (Join-Path $testRoot 'RedirectedBackups') -Confirm:$false
    } 'Installer accepted a junction as the VST3 destination root.'
    Assert (-not (Test-Path -LiteralPath (Join-Path $outsideVst3 'SkellCast.vst3'))) 'Installer wrote through a redirected destination root.'
    Remove-TestJunction $redirectedVst3

    # A redirected backup root must not receive a backup or allow an update to proceed.
    $backupRootTarget = Join-Path $testRoot 'outside-backup-root'
    New-Item -ItemType Directory -Path $backupRootTarget -Force | Out-Null
    $redirectedBackupRoot = Join-Path $testRoot 'redirected-backup-root'
    New-TestJunction $redirectedBackupRoot $backupRootTarget
    $installedModule = Join-Path $target 'SkellCast.vst3\Contents\x86_64-win\SkellCast.vst3'
    $moduleBeforeBackupGuard = Get-Hash $installedModule
    Invoke-ExpectedFailure {
        & $installer -PackageRoot $packageV2 -Vst3Directory $target -BackupRoot $redirectedBackupRoot -Confirm:$false
    } 'Installer accepted a junction as the backup root during update.'
    Assert ((Get-Hash $installedModule) -eq $moduleBeforeBackupGuard) 'Rejected backup-root junction changed the installed module.'
    Assert (@(Get-ChildItem -LiteralPath $backupRootTarget -Force).Count -eq 0) 'Installer wrote backups through a redirected backup root.'
    Remove-TestJunction $redirectedBackupRoot

    $script:passed = $true
    Write-Host "SkellCast install/update/rollback tests: $script:assertions assertions passed."
}
finally {
    if ($script:passed -and (Test-Path -LiteralPath $testRoot -PathType Container)) {
        $rootItem = Get-Item -LiteralPath $testRoot -Force
        $markerItem = if (Test-Path -LiteralPath $marker -PathType Leaf) { Get-Item -LiteralPath $marker -Force } else { $null }
        $nested = @(Get-ChildItem -LiteralPath $testRoot -Recurse -Force -ErrorAction Stop |
            Where-Object { ($_.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -ne 0 })
        if (($rootItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -eq 0 -and
            $null -ne $markerItem -and
            ($markerItem.Attributes -band [System.IO.FileAttributes]::ReparsePoint) -eq 0 -and
            ([System.IO.File]::ReadAllText($marker) -ceq $token) -and
            $nested.Count -eq 0) {
            Remove-Item -LiteralPath $testRoot -Recurse -Force -ErrorAction Stop
        }
        else {
            Write-Warning "Test root ownership could not be verified; preserving it at $testRoot"
        }
    }
    elseif (-not $script:passed) {
        Write-Warning "Tests failed; preserving the owned test directory for diagnosis: $testRoot"
    }
}
