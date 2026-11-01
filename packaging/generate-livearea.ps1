# SPDX-License-Identifier: CC0-1.0
param([string]$Python = 'python')
$ErrorActionPreference='Stop'
$repoRoot=Split-Path $PSScriptRoot -Parent
$metadata=Get-Content -LiteralPath "$repoRoot/VERSION.json" -Raw | ConvertFrom-Json
& $Python "$repoRoot/tools/make_final_livearea.py" --font "$repoRoot/src/vcmi/Mods/vcmi/Content/Data/NotoSerif-Bold.ttf" --out $PSScriptRoot --rev $metadata.livearea_content_rev
if ($LASTEXITCODE) { throw 'LiveArea generator failed.' }
& "$PSScriptRoot/generate-boot.ps1"
