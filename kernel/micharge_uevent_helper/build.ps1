param(
    [Parameter(Mandatory = $true)]
    [string]$KernelDir,

    [string]$OutDir = "",
    [string]$CrossCompile = "aarch64-linux-android-",
    [switch]$NoWsl
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $OutDir) {
    $OutDir = Join-Path $ScriptDir "out"
}

if (-not (Test-Path -LiteralPath $KernelDir)) {
    throw "KernelDir not found: $KernelDir"
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if ($NoWsl) {
    make -C $KernelDir M=$ScriptDir ARCH=arm64 LLVM=1 CROSS_COMPILE=$CrossCompile modules
} else {
    $KernelDirWsl = (wsl.exe wslpath -a "$KernelDir").Trim()
    $ScriptDirWsl = (wsl.exe wslpath -a "$ScriptDir").Trim()
    wsl.exe --cd "$ScriptDirWsl" make -C "$KernelDirWsl" M="$ScriptDirWsl" ARCH=arm64 LLVM=1 CROSS_COMPILE="$CrossCompile" modules
}

if ($LASTEXITCODE -ne 0) {
    throw "kernel module build failed with exit code $LASTEXITCODE"
}

$Ko = Join-Path $ScriptDir "micharge_uevent.ko"
if (-not (Test-Path -LiteralPath $Ko)) {
    throw "module was not produced: $Ko"
}

Copy-Item -Force -LiteralPath $Ko -Destination (Join-Path $OutDir "micharge_uevent.ko")
Write-Host "Built $(Join-Path $OutDir 'micharge_uevent.ko')"
