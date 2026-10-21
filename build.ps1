# Build & run Gamecharacterlike_desktoppet (Qt 6.11.0 MinGW 64-bit)
#
#   .\build.ps1          # configure + build
#   .\build.ps1 -Run     # configure + build, then launch
#   .\build.ps1 -Clean   # remove the build directory

param(
    [switch]$Run,
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$QtRoot   = 'D:\Programing\Qt'
$QtDir    = Join-Path $QtRoot '6.11.0\mingw_64'
$MingwBin = Join-Path $QtRoot 'Tools\mingw1310_64\bin'
$CMake    = Join-Path $QtRoot 'Tools\CMake_64\bin\cmake.exe'

$Src   = $PSScriptRoot
$Build = Join-Path $Src 'build'

if ($Clean) {
    if (Test-Path $Build) { Remove-Item $Build -Recurse -Force }
    Write-Host "Cleaned $Build"
    return
}

$env:PATH = "$MingwBin;$QtDir\bin;$env:PATH"

& $CMake -S $Src -B $Build -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Debug `
    -DCMAKE_PREFIX_PATH="$QtDir" `
    -DCMAKE_MAKE_PROGRAM="$MingwBin\mingw32-make.exe" `
    -DCMAKE_CXX_COMPILER="$MingwBin\g++.exe"
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }

& $CMake --build $Build -j 8
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }

$Exe = Join-Path $Build 'Gamecharacterlike_desktoppet.exe'
Write-Host "Built: $Exe"

if ($Run) { Start-Process $Exe }
