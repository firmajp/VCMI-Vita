# SPDX-License-Identifier: GPL-3.0-or-later
$ErrorActionPreference='Stop'
if (!$env:VITASDK) { throw 'Set VITASDK to the dedicated SDK used for this build.' }
$env:PATH="$env:VITASDK/bin;$env:PATH"
& python "$PSScriptRoot/test_lgpl_relink.py"
if ($LASTEXITCODE) { throw 'Modified LGPL source/relink test failed.' }
