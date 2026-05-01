# GKI module build environment

Target device kernel:

```text
6.12.23-android16-5-gf1bdb13583da-ab13761046-4k
```

This maps to Android GKI build `13761046`, target `kernel_aarch64`, branch
`aosp_kernel-common-android16-6.12-2025-06`. The matching public release tag is
`android16-6.12-2025-06_r8`.

## WSL environment

The minimal WSL distro used here is `Ubuntu-24.04`.

Installed packages:

```bash
build-essential git curl ca-certificates unzip xz-utils bc bison flex \
libssl-dev libelf-dev libdw-dev dwarves rsync python3 python3-pip clang lld llvm \
pahole golang-go bazel-bootstrap
```

Toolchain sanity check:

```bash
clang --version
make --version
git --version
pahole --version
```

## Downloaded GKI artifacts

The setup script downloads real artifacts from Android CI by opening the CI
artifact viewer page and following its signed `artifactUrl`.

Local WSL layout:

```text
/opt/android-gki/13761046/artifacts/
  init_ddk.zip
  kernel_aarch64_Module.symvers
  kernel_aarch64_dot_config
  kernel_aarch64_ddk_headers_archive.tar.gz
  ci_target_mapping.json
  repo.prop
  build.config.constants

/opt/android-gki/13761046/headers/
  common/...

/opt/android-gki/13761046/ddk-workspace/
  MODULE.bazel
```

To reproduce:

```powershell
.\kernel\micharge_uevent_helper\setup_gki_ddk.ps1
```

## Current limitation

The environment is now ready for Android's DDK/Kleaf flow, but the generated
`ddk-workspace` still needs the Kleaf repo to actually build a `ddk_module`
target. The `init_ddk.zip --nosync` mode intentionally skips fetching Kleaf.

The direct `android.googlesource.com` connection timed out in this environment,
so I did not run `repo sync` for Kleaf yet. Once that network path is available,
run `init_ddk.zip` without `--nosync`, or pass a local Kleaf checkout with
`--local --kleaf_repo <path>`.

I also tried installing Bazelisk through `go install`, but `proxy.golang.org`
timed out. As a fallback, Ubuntu's `bazel-bootstrap` package is installed and
`bazel --version` returns `bazel no_version`. That is enough to confirm a Bazel
binary exists, but Android's current Kleaf flow may still prefer Bazelisk or a
specific Bazel version once the Kleaf repo is available.

The older helper script:

```powershell
.\kernel\micharge_uevent_helper\build.ps1 -KernelDir <prepared-kernel-build-dir>
```

still works when `<prepared-kernel-build-dir>` is a full, prepared Kbuild output
tree with generated headers and `Module.symvers`. The DDK prebuilts above are
not that traditional Kbuild tree; they are meant for Kleaf/Bazel.

## Traditional Kbuild fallback

When WSL cannot use the Windows localhost proxy directly, download the GKI
source archive from Windows with the local proxy and prepare a Kbuild tree:

```powershell
curl.exe -L --proxy http://127.0.0.1:7897 `
  -o .\downloads\android16-6.12-2025-06_r8.tar.gz `
  https://android.googlesource.com/kernel/common/+archive/refs/tags/android16-6.12-2025-06_r8.tar.gz

wsl.exe -d Ubuntu-24.04 -u root -- bash /mnt/c/Users/GUF296/Documents/workspace/micharge/tools/prepare_gki_kbuild.sh
wsl.exe -d Ubuntu-24.04 -u root -- bash /mnt/c/Users/GUF296/Documents/workspace/micharge/tools/fix_gki_localversion.sh
wsl.exe -d Ubuntu-24.04 -u root -- bash /mnt/c/Users/GUF296/Documents/workspace/micharge/tools/build_micharge_uevent_wsl.sh
```

The compiled module is staged at:

```text
out/staging/vendor_dlkm/lib/modules/micharge_uevent.ko
```
