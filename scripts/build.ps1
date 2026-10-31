# SPDX-License-Identifier: GPL-3.0-or-later
param([int]$Jobs=2, [string]$Shell='sh', [string]$Make='make', [switch]$SkipDependencies, [switch]$BuildOnly, [switch]$ResumeDependencies)
$ErrorActionPreference='Stop'
if (!$env:VITASDK) { throw 'Set VITASDK to a dedicated VitaSDK installation.' }
$repoRoot=(Split-Path $PSScriptRoot -Parent) -replace '\\','/'
$sdkRoot=(Resolve-Path $env:VITASDK).Path -replace '\\','/'
$env:PATH="$sdkRoot/bin;$env:PATH"
$toolchain="$sdkRoot/share/vita.toolchain.cmake"
$prefix="$sdkRoot/arm-vita-eabi"
$mapFlags="-ffile-prefix-map=$repoRoot=/vcmi-vita -fdebug-prefix-map=$repoRoot=/vcmi-vita -ffile-prefix-map=$sdkRoot=/vitasdk -fdebug-prefix-map=$sdkRoot=/vitasdk"
function Run([string]$Program,[string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE) { throw "$Program failed with exit code $LASTEXITCODE" }
}
function BuildDependency([string]$Name,[string[]]$Options) {
    Run cmake (@('-S',"$repoRoot/third_party/$Name",'-B',"$repoRoot/build/$Name",'-G','Ninja',
        "-DCMAKE_TOOLCHAIN_FILE=$toolchain","-DCMAKE_INSTALL_PREFIX=$prefix",'-DCMAKE_BUILD_TYPE=Release',
        '-DCMAKE_POLICY_VERSION_MINIMUM=3.5','-DBUILD_SHARED_LIBS=OFF','-DCMAKE_POSITION_INDEPENDENT_CODE=OFF',"-DCMAKE_ASM_FLAGS=$mapFlags","-DCMAKE_C_FLAGS=$mapFlags -Wno-error=implicit-function-declaration -Wno-error=int-conversion -Wno-error=incompatible-pointer-types",
        "-DCMAKE_CXX_FLAGS=$mapFlags")+$Options)
    Run cmake @('--build',"$repoRoot/build/$Name",'--parallel',"$Jobs")
    if ($Name -ne 'vita-iconv') { Run cmake @('--install',"$repoRoot/build/$Name") }
}
if (!$SkipDependencies) {
    BuildDependency pthread-embedded @('-DBUILD_TESTSUITE=OFF')
    BuildDependency SDL2 @('-DSDL_TEST=OFF','-DSDL_SHARED=OFF','-DSDL_STATIC=ON','-DSDL_ARMNEON=ON','-DSDL_ARMSIMD=ON','-DARMNEON_FOUND=TRUE','-DARMSIMD_FOUND=TRUE')
    if (!$ResumeDependencies -or !(Test-Path "$repoRoot/build/ffmpeg-prefix/lib/libavcodec.a")) {
        & "$PSScriptRoot/build-ffmpeg.ps1" -Jobs $Jobs -Shell $Shell -Make $Make
    }
    BuildDependency oneTBB @('-DTBB_TEST=OFF','-DTBBMALLOC_BUILD=OFF','-DTBB_EXAMPLES=OFF','-DTBB_STRICT=OFF','-DTBB_DISABLE_HWLOC_AUTOMATIC_SEARCH=ON')
    BuildDependency libsquish @('-DBUILD_SQUISH_WITH_SSE2=OFF','-DBUILD_SQUISH_WITH_ALTIVEC=OFF','-DBUILD_SQUISH_EXTRA=OFF')
    BuildDependency vita-iconv @()
    Copy-Item -LiteralPath "$repoRoot/build/vita-iconv/libvcmi_vita_iconv.a" -Destination "$prefix/lib/libvcmi_vita_iconv.a"
    BuildDependency minizip @('-DMZ_COMPAT=ON','-DMZ_ZLIB=ON','-DMZ_BZIP2=OFF','-DMZ_LZMA=OFF','-DMZ_PPMD=OFF','-DMZ_ZSTD=OFF','-DMZ_FETCH_LIBS=OFF','-DMZ_PKCRYPT=OFF','-DMZ_WZAES=OFF','-DMZ_OPENSSL=OFF','-DMZ_ICONV=OFF','-DMZ_BUILD_TESTS=OFF','-DMZ_BUILD_UNIT_TESTS=OFF')
}
$ffmpegOptions=@()
foreach ($component in @('avutil','swscale','avformat','avcodec','swresample')) {
    $ffmpegOptions+="-Dffmpeg_${component}_INCLUDE_DIR=$repoRoot/build/ffmpeg-prefix/include"
    $ffmpegOptions+="-Dffmpeg_${component}_LIBRARY=$repoRoot/build/ffmpeg-prefix/lib/lib${component}.a"
}
Run cmake (@('-S',"$repoRoot/src/vcmi",'-B',"$repoRoot/build/vcmi",'-G','Ninja',
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain",'-DCMAKE_BUILD_TYPE=Release',"-DCMAKE_C_FLAGS=$mapFlags","-DCMAKE_CXX_FLAGS=-Wl,-q $mapFlags",'-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
    '-DENABLE_STATIC_LIBS=ON','-DBoost_USE_STATIC_LIBS=ON','-DBoost_USE_STATIC_RUNTIME=ON','-DIconv_IS_BUILT_IN=TRUE',
    '-DENABLE_PCH=OFF','-DENABLE_MONOLITHIC_INSTALL=ON','-DENABLE_GITVERSION=OFF','-DCOPY_CONFIG_ON_BUILD=OFF','-DENABLE_CLIENT=ON','-DENABLE_SERVER=OFF',
    '-DENABLE_LAUNCHER=OFF','-DENABLE_EDITOR=OFF','-DENABLE_TEST=OFF','-DENABLE_VIDEO=ON',
    '-DENABLE_LOBBY=OFF','-DENABLE_DISCORD=OFF','-DENABLE_MMAI=OFF','-DENABLE_NULLKILLER2_AI=ON','-DENABLE_TRANSLATIONS=OFF')+$ffmpegOptions)
Run cmake @('--build',"$repoRoot/build/vcmi",'--target','clientapp/vcmiclient.vpk','--parallel',"$Jobs")
if (!$BuildOnly) {
    Run python @("$PSScriptRoot/package.py")
    Run python @("$PSScriptRoot/audit.py")
}




