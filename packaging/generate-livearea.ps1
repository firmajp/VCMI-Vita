# SPDX-License-Identifier: CC0-1.0
# Wrapper for the CC0 LiveArea generator. Boot splash is generated separately.
param([string]$Python = 'python')
$ErrorActionPreference='Stop'
$repoRoot=Split-Path $PSScriptRoot -Parent
& $Python "$repoRoot/tools/make_vcmi_livearea.py" --font "$repoRoot/src/vcmi/Mods/vcmi/Content/Data/NotoSerif-Bold.ttf" --out $PSScriptRoot --rev 101
if ($LASTEXITCODE) { throw 'LiveArea generator failed.' }
