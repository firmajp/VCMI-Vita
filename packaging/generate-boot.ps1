# SPDX-License-Identifier: CC0-1.0
$ErrorActionPreference='Stop'
Copy-Item -LiteralPath "$PSScriptRoot/sce_sys/pic0.png" -Destination "$PSScriptRoot/vitaBoot.png"
Write-Output 'Boot/loading background copied byte-for-byte from the 960x544 launch image.'
