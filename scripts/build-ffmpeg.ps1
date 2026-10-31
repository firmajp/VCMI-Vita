# SPDX-License-Identifier: GPL-3.0-or-later
param([int]$Jobs=2, [string]$Shell='sh', [string]$Make='make')
$ErrorActionPreference='Stop'
if (!$env:VITASDK) { throw 'Set VITASDK first.' }
$repoRoot=Split-Path $PSScriptRoot -Parent
$mappedRoot=$repoRoot -replace '\\','/'
$mappedSdk=(Resolve-Path $env:VITASDK).Path -replace '\\','/'
$env:VITA_FFMPEG_CFLAGS="-D_BSD_SOURCE -ffile-prefix-map=$mappedRoot=/vcmi-vita -fdebug-prefix-map=$mappedRoot=/vcmi-vita -ffile-prefix-map=$mappedSdk=/vitasdk -fdebug-prefix-map=$mappedSdk=/vitasdk"
$env:PATH="$env:VITASDK/bin;$env:PATH"
$env:VITA_FFMPEG_CC=(($env:VITASDK -replace '\\','/')+'/bin/arm-vita-eabi-gcc')
if ($IsWindows -or $env:OS -eq 'Windows_NT') { $env:VITA_FFMPEG_CC += '.exe' }
New-Item -ItemType Directory -Force "$repoRoot/build/ffmpeg", "$repoRoot/build/tmp" | Out-Null
$env:TMPDIR=(Resolve-Path "$repoRoot/build/tmp").Path -replace '\\','/'
$env:TEMP=$env:TMPDIR
$env:TMP=$env:TMPDIR
Push-Location "$repoRoot/build/ffmpeg"
try {
    & $Shell -c 'sh ../../third_party/FFmpeg/configure --enable-cross-compile --cc=arm-vita-eabi-gcc --host-cc=arm-vita-eabi-gcc --ar=arm-vita-eabi-ar --nm=arm-vita-eabi-nm --ranlib=arm-vita-eabi-ranlib --strip=arm-vita-eabi-strip --arch=arm --cpu=cortex-a9 --target-os=none --disable-everything --disable-autodetect --disable-programs --disable-doc --disable-network --disable-avdevice --disable-avfilter --disable-postproc --disable-shared --enable-static --disable-debug --disable-pthreads --disable-asm --enable-decoder=bink,smacker,binkaudio_dct,binkaudio_rdft,smackaud,pcm_s16le,pcm_u8 --enable-demuxer=bink,smacker --extra-cflags="$VITA_FFMPEG_CFLAGS" --prefix=../ffmpeg-prefix'
    if ($LASTEXITCODE) { throw 'FFmpeg configure failed' }
    Add-Content ffbuild/config.mak -Value "`nCFLAGS += -fno-short-enums"
    & python "$PSScriptRoot/sanitize_ffmpeg_config.py" config.h $mappedRoot $mappedSdk
    if ($LASTEXITCODE) { throw 'FFmpeg configuration path normalization failed' }
    & $Make -j $Jobs "SHELL=$Shell" '.SHELLFLAGS=-c'
    if ($LASTEXITCODE) { throw 'FFmpeg build failed' }
    & $Make install "SHELL=$Shell" '.SHELLFLAGS=-c'
    if ($LASTEXITCODE) { throw 'FFmpeg install failed' }
} finally { Pop-Location }
& python "$PSScriptRoot/normalize_ffmpeg_headers.py" "$repoRoot/build/ffmpeg-prefix/include"
if ($LASTEXITCODE) { throw 'FFmpeg header adaptation failed' }

