# SPDX-License-Identifier: GPL-3.0-or-later
param([Parameter(Mandatory=$true)][string]$RepositoryPath)
$ErrorActionPreference='Stop'
$root=(Resolve-Path -LiteralPath $RepositoryPath).Path.TrimEnd('\','/')
$manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'UPDATE_MANIFEST.json') -Raw | ConvertFrom-Json
function SafePath([string]$Base,[string]$Relative) {
    if ([IO.Path]::IsPathRooted($Relative) -or $Relative -match '(^|[\\/])\.\.([\\/]|$)|(^|[\\/])\.git([\\/]|$)|:') { throw "Unsafe update path: $Relative" }
    $prefix=[IO.Path]::GetFullPath($Base).TrimEnd('\','/')+[IO.Path]::DirectorySeparatorChar
    $path=[IO.Path]::GetFullPath((Join-Path $Base $Relative))
    if (!$path.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)) { throw "Path outside target: $Relative" }
    return $path
}
function BlobHash([string]$Path) {
    $bytes=[IO.File]::ReadAllBytes($Path)
    $header=[Text.Encoding]::UTF8.GetBytes("blob $($bytes.Length)`0")
    $hasher=[Security.Cryptography.SHA1]::Create()
    try {
        [void]$hasher.TransformBlock($header,0,$header.Length,$header,0)
        [void]$hasher.TransformFinalBlock($bytes,0,$bytes.Length)
        return ([BitConverter]::ToString($hasher.Hash)).Replace('-','').ToLowerInvariant()
    } finally { $hasher.Dispose() }
}
if (!(Test-Path -LiteralPath (Join-Path $root '.git') -PathType Container)) { throw 'Choose the existing Git repository directory.' }
$gitDir=Join-Path $root '.git'
$head=& git "--git-dir=$gitDir" rev-parse HEAD
if ($LASTEXITCODE -or $head.Trim() -ne $manifest.baseline_commit) { throw 'Repository HEAD differs from this update baseline; do not apply automatically.' }
$operations=@()
foreach ($item in $manifest.files) {
    $target=SafePath $root $item.path
    if ($item.action -ne 'delete') {
        $source=SafePath (Join-Path $PSScriptRoot 'files') $item.path
        if ((Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -ne $item.sha256) { throw "Corrupt update payload: $($item.path)" }
        if ((Test-Path -LiteralPath $target -PathType Leaf) -and (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant() -eq $item.sha256) { continue }
    }
    if (Test-Path -LiteralPath $target) {
        if (!(Test-Path -LiteralPath $target -PathType Leaf)) { throw "Expected a file: $($item.path)" }
        if (!$item.baseline_git_blob -or (BlobHash $target) -ne $item.baseline_git_blob) { throw "Local changes detected; no files changed: $($item.path)" }
    } elseif ($item.action -eq 'delete') { continue }
    elseif ($item.baseline_git_blob) { throw "Missing baseline file; no files changed: $($item.path)" }
    $operations+=@{Item=$item;Target=$target}
}
$backup=Join-Path ([IO.Path]::GetTempPath()) ('VCMI-Vita-before-1.1.0-'+[Guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $backup)
foreach ($op in $operations) {
    if (Test-Path -LiteralPath $op.Target -PathType Leaf) {
        $saved=SafePath $backup $op.Item.path
        [void](New-Item -ItemType Directory -Force -Path (Split-Path $saved -Parent))
        Copy-Item -LiteralPath $op.Target -Destination $saved
    }
}
foreach ($op in $operations) {
    if ($op.Item.action -eq 'delete') { Remove-Item -LiteralPath $op.Target -Force }
    else {
        [void](New-Item -ItemType Directory -Force -Path (Split-Path $op.Target -Parent))
        Copy-Item -LiteralPath (SafePath (Join-Path $PSScriptRoot 'files') $op.Item.path) -Destination $op.Target -Force
    }
}
Write-Output "Applied $($operations.Count) file changes. Backup: $backup"
Write-Output 'Review the changes in GitHub Desktop. This script does not stage, commit, push or publish.'
