param(
    [string]$Distro = "Ubuntu-24.04",
    [string]$BuildId = "13761046",
    [string]$Target = "kernel_aarch64",
    [string]$Root = "/opt/android-gki/13761046"
)

$ErrorActionPreference = "Stop"

$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$Downloader = Join-Path $RepoRoot "tools\download_android_ci_artifact.py"
$DownloaderWsl = (wsl.exe -d $Distro -u root -- wslpath -a "$Downloader").Trim()

function Invoke-WslBash([string]$Command) {
    wsl.exe -d $Distro -u root -- bash -lc $Command
    if ($LASTEXITCODE -ne 0) {
        throw "WSL command failed with exit code $LASTEXITCODE"
    }
}

Invoke-WslBash "DEBIAN_FRONTEND=noninteractive apt-get update"
Invoke-WslBash "DEBIAN_FRONTEND=noninteractive apt-get install -y build-essential git curl ca-certificates unzip xz-utils bc bison flex libssl-dev libelf-dev libdw-dev dwarves rsync python3 python3-pip clang lld llvm pahole golang-go bazel-bootstrap"
Invoke-WslBash "mkdir -p '$Root/artifacts' '$Root/ddk-workspace' '$Root/headers'"

$Artifacts = @(
    "init_ddk.zip",
    "kernel_aarch64_Module.symvers",
    "kernel_aarch64_dot_config",
    "kernel_aarch64_ddk_headers_archive.tar.gz",
    "ci_target_mapping.json",
    "repo.prop",
    "build.config.constants"
)

foreach ($Artifact in $Artifacts) {
    Invoke-WslBash "python3 '$DownloaderWsl' '$BuildId' '$Target' '$Artifact' '$Root/artifacts/$Artifact'"
}

Invoke-WslBash "rm -rf '$Root/headers' && mkdir -p '$Root/headers' && tar -xzf '$Root/artifacts/kernel_aarch64_ddk_headers_archive.tar.gz' -C '$Root/headers'"
Invoke-WslBash "cd '$Root' && python3 artifacts/init_ddk.zip --build_id '$BuildId' --build_target '$Target' --ddk_workspace '$Root/ddk-workspace' --prebuilts_dir '$Root/artifacts' --nosync"

Write-Host "GKI DDK prebuilts are ready at $Root"
Write-Host "DDK workspace: $Root/ddk-workspace"
Write-Host "Headers archive extracted to: $Root/headers"
