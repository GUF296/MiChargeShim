param(
    [string]$Ndk = "C:\Users\GUF296\Documents\workspace\android-ndk-r27d",
    [int]$Api = 35
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$OutDir = Join-Path $ScriptDir "out"
$Clang = Join-Path $Ndk "toolchains\llvm\prebuilt\windows-x86_64\bin\aarch64-linux-android$Api-clang++.cmd"

if (-not (Test-Path -LiteralPath $Clang)) {
    throw "clang++ not found: $Clang"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$Source = Join-Path $ScriptDir "service.cpp"
$Output = Join-Path $OutDir "vendor.xiaomi.hardware.micharge-service"

& $Clang `
    -std=c++17 `
    -Wall `
    -Wextra `
    -Werror `
    -O2 `
    -fPIE `
    -pie `
    -static-libstdc++ `
    $Source `
    -o $Output `
    -llog `
    -lbinder_ndk `
    -ldl

if ($LASTEXITCODE -ne 0) {
    throw "clang++ failed with exit code $LASTEXITCODE"
}

Write-Host "Built $Output"
