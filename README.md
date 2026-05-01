# MiChargeShim

A portable implementation of the Xiaomi MiCharge battery charging system HAL for Android devices. This project includes a userspace HAL shim and kernel module for emitting charging state information via uevents.

## Overview

MiChargeShim bridges Xiaomi/HyperOS battery charging framework code with generic Android power supply subsystems. It provides:

- **HAL Shim** (`service.cpp`): A drop-in replacement for `/vendor/bin/hw/vendor.xiaomi.hardware.micharge-service` that implements the `vendor.xiaomi.hardware.micharge` AIDL interface
- **Kernel Module** (`micharge_uevent.c`): A Linux kernel module that translates HAL state into sysfs interface and power supply uevents
- **Documentation**: Comprehensive analysis and build instructions

## Architecture

```
┌─────────────────────────────────────────────┐
│      SystemUI / MiuiBatteryServiceImpl       │
│  (listens for POWER_SUPPLY_QUICK_CHARGE_TYPE)
└──────────────────┬──────────────────────────┘
                   │ KOBJ_CHANGE uevent
                   ▼
    ┌──────────────────────────────┐
    │   Kernel Module              │
    │ (micharge_uevent.ko)         │
    │ /sys/kernel/micharge_uevent  │
    └──────────────────┬───────────┘
                       │ sysfs write
                       ▼
┌──────────────────────────────────────────────┐
│          HAL Shim Service                    │
│ vendor.xiaomi.hardware.micharge-service      │
│  ├─ getBatteryAuthentic()                    │
│  ├─ getBatteryCapacity()                     │
│  ├─ getBatteryChargeType()                   │
│  └─ 8 more methods...                        │
└─────────────────────────────────────────────┘
```

## Features

### HAL Shim Features
- Replaces the original Xiaomi HAL service binary
- Implements all 16 methods of the `IMiCharge` AIDL interface v2
- Reads from standard sysfs power supply nodes
- Supports optional charging prediction libraries
- Statically linked C++ runtime (no additional dependencies)

### Kernel Module Features
- Bridges HAL shim to framework uevents
- Emits `POWER_SUPPLY_QUICK_CHARGE_TYPE` changes
- Works with any kernel that has power supply subsystem
- Minimal overhead (~500 lines of code)

## Building

### Prerequisites

- Android NDK r27 or later
- PowerShell 5.1 or later (for build scripts)
- Linux kernel headers (for kernel module)

### Build HAL Shim (Windows/PowerShell)

```powershell
cd shim\micharge
.\build.ps1
```

Output: `shim/micharge/out/vendor.xiaomi.hardware.micharge-service`

If NDK is in a different location:
```powershell
.\build.ps1 -Ndk "C:\path\to\android-ndk-r27d"
```

### Build Kernel Module

Requires the target kernel's build tree. On the development machine:

```powershell
cd kernel\micharge_uevent_helper
.\build.ps1 -KernelDir "C:\path\to\kernel\out"
```

Output: `kernel/micharge_uevent_helper/micharge_uevent.ko`

**Note**: The module must be built against the exact kernel version running on the target device.

## Installation

> **Note**: Modern Android devices use verified boot (AVB2) and dynamic partitions. The legacy `adb remount` approach no longer works on production devices. You must unpack, modify, and repack the ROM image.

### Prerequisites for Image Repackaging

- **Python 3.7+** with required packages
- **Android image tools**:
  ```bash
  # Linux/macOS
  git clone https://github.com/topjohnwu/Magisk
  # Or use pre-built: https://github.com/averyOS/avery-tools
  
  # Windows (recommended): Use WSL2 or Docker
  ```

### Method 1: Using `super.img` (Dynamic Partitions)

Most modern Xiaomi devices use A/B dynamic partitions in `super.img`.

#### Step 1: Extract Partitions

```bash
# Install lpunpack
git clone https://android.googlesource.com/platform/system/tools/mkbootimg
cd mkbootimg
python3 mkbootimg/unpack_bootimg.py --boot_img boot.img --out boot_unpacked/

# Unpack super.img
python3 -m pip install lptools
lpunpack super.img output_dir/

# Extract vendor image
mkdir -p vendor_extracted
cd vendor_extracted
simg2img ../output_dir/vendor.img vendor.img.raw
mount -o loop vendor.img.raw mnt/
```

#### Step 2: Replace HAL Service and Kernel Module

```bash
# Backup original
cp mnt/bin/hw/vendor.xiaomi.hardware.micharge-service \
   vendor.xiaomi.hardware.micharge-service.orig

# Replace HAL shim
sudo cp shim/micharge/out/vendor.xiaomi.hardware.micharge-service \
       mnt/bin/hw/vendor.xiaomi.hardware.micharge-service
sudo chmod 755 mnt/bin/hw/vendor.xiaomi.hardware.micharge-service

# Copy kernel module and init script
sudo mkdir -p mnt/etc/init/
sudo mkdir -p mnt/../vendor_dlkm/lib/modules/
sudo cp kernel/micharge_uevent_helper/micharge_uevent.ko \
       mnt/../vendor_dlkm/lib/modules/
sudo cp kernel/micharge_uevent_helper/init.micharge_uevent.rc \
       mnt/etc/init/init.micharge_uevent.rc
```

#### Step 3: Fix SELinux Labels

The HAL service and init script must have correct SELinux contexts:

```bash
# Check original contexts
ls -Z mnt/bin/hw/ | grep micharge

# Common contexts for HAL services:
# system_file (for binaries in /system)
# vendor_file (for binaries in /vendor)
# init_exec (for rc files)

# Using chcon (if available)
sudo chcon -h system_u:object_r:system_file:s0 \
    mnt/bin/hw/vendor.xiaomi.hardware.micharge-service

# Or edit file_contexts if available
# Append to vendor_file_contexts or system/sepolicy/file_contexts
```

#### Step 4: Repack Image

```bash
# Unmount and repack
cd vendor_extracted
sudo umount mnt/
img2simg vendor.img.raw vendor.img
sudo chown $(id -u):$(id -g) vendor.img

# Repack super.img
lpmake --metadata-size 65536 \
       --super-name super \
       --metadata-slot 0 \
       --virtual-ab \
       -P vendor /path/to/vendor.img \
       -P system /path/to/system.img \
       -P system_ext /path/to/system_ext.img \
       -o super.img.new

# Or use lpunpack in reverse (tool-dependent)
```

### Method 2: Using Fastboot Flash (Recommended)

If you have access to the original firmware structure:

```bash
# Extract from factory ROM
unzip MIUI_ROM.zip

# Unpack boot image
cd BOOT
unpack_bootimg --boot_img=boot.img
# Modify ramdisk as needed...

# Unpack vendor image
mkdir vendor_work
cd vendor_work
simg2img ../VENDOR vendor.img.raw
mount -o loop vendor.img.raw mnt/

# Make modifications (same as Method 1 Step 2-3)
...

# Repack and flash
sudo umount mnt/
img2simg vendor.img.raw vendor.img

# Flash via fastboot
fastboot flash vendor vendor.img
fastboot reboot
```

### Method 3: Build Custom ROM with Recovery

If building from AOSP/Xiaomi source:

```bash
# Copy files to source tree
cp shim/micharge/out/vendor.xiaomi.hardware.micharge-service \
   device/xiaomi/[device]/prebuilt/system/bin/hw/

cp kernel/micharge_uevent_helper/micharge_uevent.ko \
   device/xiaomi/[device]/prebuilt/vendor_dlkm/lib/modules/

cp kernel/micharge_uevent_helper/init.micharge_uevent.rc \
   device/xiaomi/[device]/etc/init/

# SELinux context must be defined in:
# device/xiaomi/[device]/sepolicy/vendor/file_contexts
# device/xiaomi/[device]/sepolicy/vendor/micharge.te (if needed)

# Build
lunch [device]-user
m -j$(nproc)
```

### Verifying Installation

After flashing or rebooting:

```bash
# Check HAL service
adb shell getprop init.svc.vendor.xiaomi.hardware.micharge-service

# Check kernel module
adb shell lsmod | grep micharge

# Check SELinux contexts
adb shell ls -Z /vendor/bin/hw/vendor.xiaomi.hardware.micharge-service
adb shell ls -Z /vendor/etc/init/init.micharge_uevent.rc

# Verify no denial logs
adb logcat | grep -i micharge
adb logcat | grep -i "type=1400.*denied" | grep micharge
```

## Verification

Check if services are running:

```bash
adb shell getprop init.svc.vendor.xiaomi.hardware.micharge-service
```

Monitor uevents:

```bash
adb shell uevents | grep POWER_SUPPLY_QUICK_CHARGE_TYPE
```

View logcat output:

```bash
adb logcat -b all -v threadtime -s MiChargeShim '*:S'
```

## Documentation

- [Architecture & Analysis](docs/micharge-analysis.md) - Deep dive into the original HAL implementation
- [Kernel Module Details](kernel/micharge_uevent_helper/README.md) - How the kernel bridge works
- [Build Environment Setup](docs/gki-module-build-env.md) - GKI kernel development environment
- [Test Plan](docs/test-plan.md) - Testing procedures

## HAL Interface

The `IMiCharge` AIDL interface (v2, hash: `f8886a701012d522ea9977256f52b3c47596963d`) provides:

| Method | Returns | Purpose |
|--------|---------|---------|
| `getBatteryAuthentic()` | String | Battery authenticity status |
| `getBatteryCapacity()` | String | Current capacity (0-100%) |
| `getBatteryChargeFull()` | String | Full charge capacity |
| `getBatteryChargeType()` | String | Charging method (QC, PD, etc.) |
| `getBatteryCycleCount()` | String | Total charge cycles |
| `getBatteryIbat()` | String | Current draw (mA) |
| `getBatteryResistance()` | String | Internal resistance (Ω) |
| `getBatterySoh()` | String | State of health (%) |
| `getBatteryTbat()` | String | Battery temperature (°C) |
| `getBatteryThermaLevel()` | String | Thermal throttling level |
| `getBatteryVbat()` | String | Voltage (mV) |
| `getBatteryWarrantyStatus()` | String | Warranty status |
| `getQuickChargeType()` | String | Fast charging type (0-4) |
| `setSmartChargeCmd()` | void | Smart charge control |
| `setBatteryDefenderCmd()` | void | Battery protection commands |
| `getVersionBatteryDefender()` | String | Defender version |

## sysfs Interface (Kernel Module)

The module creates these sysfs entries under `/sys/kernel/micharge_uevent/`:

```
/sys/kernel/micharge_uevent/quick_charge_type    (write to emit uevent)
/sys/kernel/micharge_uevent/power_max             (power limit in watts)
/sys/kernel/micharge_uevent/online                (charger present)
/sys/kernel/micharge_uevent/usb_type              (USB/PD/HVDCP/etc)
```

## Sample Output

When charging state changes:

```
KOBJ_CHANGE@/devices/platform/power_supply/usb
ACTION=change
DEVPATH=/devices/platform/power_supply/usb
SUBSYSTEM=power_supply
POWER_SUPPLY_NAME=usb
POWER_SUPPLY_QUICK_CHARGE_TYPE=1
POWER_SUPPLY_POWER_MAX=65
POWER_SUPPLY_ONLINE=1
POWER_SUPPLY_USB_TYPE=USB_PD
```

## Compatibility

- **AOSP Versions**: Android 13 and later
- **Devices**: Any Xiaomi/Redmi device with HyperOS
- **Kernel**: Linux 5.10 and later (for kernel module)

## License

- Kernel module (`kernel/`): GPLv2 - See [COPYING](LICENSE-GPL2)
- HAL Shim & documentation: MIT - See [LICENSE](LICENSE-MIT)

Dual licensing allows the kernel module to comply with GPL while keeping the HAL shim portable.

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Troubleshooting

### Build fails with "NDK not found"
Ensure Android NDK is installed and pass the path explicitly:
```powershell
.\build.ps1 -Ndk "C:\your\ndk\path"
```

### Module fails to load
- Verify kernel version matches build target
- Check `dmesg` for symbol conflicts
- Ensure CONFIG_POWER_SUPPLY is enabled in kernel
- Check boot logs: `adb shell dmesg | grep micharge`

### HAL service crashes
- Check logcat for specific error messages: `adb logcat -s MiChargeShim`
- Verify all required sysfs nodes exist on device
- Ensure proper permissions on `/sys/kernel/micharge_uevent/`
- Check if service is properly registered: `adb shell getprop | grep micharge`

### No uevents received
- Confirm kernel module is loaded: `lsmod | grep micharge`
- Monitor with: `adb shell 'cat /proc/sys/kernel/uevent_seqnum'`
- Check sysfs write permissions: `adb shell ls -l /sys/kernel/micharge_uevent/`

### SELinux Denial Errors
If you see `type=1400` audit denials for micharge:

```bash
# Check denials
adb logcat | grep "type=1400.*denied.*micharge"

# Common issues:
# 1. Wrong file_contexts - HAL must be labeled as system_file or vendor_file
# 2. Wrong exec context - init script must be exec_type
# 3. Init process not allowed - check sepolicy/init.te

# Temporary workaround (not recommended for production):
adb shell setenforce 0    # Permissive mode (debug only)
```

### Image repacking issues
- **lpunpack fails**: Update to latest lptools: `pip install --upgrade lptools`
- **simg2img errors**: Ensure sparse image format: check with `file vendor.img`
- **Mount permission denied**: Use `sudo` or run as root
- **Sparse image corrupted**: Verify with: `simg2img vendor.img test.raw && file test.raw`
- **super.img structure**: Validate layout with `lpinfo super.img`

### Device bootloop after flashing
- Flashing incorrect vendor image format (must be sparse)
- SELinux labels prevent critical services from starting
- Kernel module incompatible with device kernel version
- **Recovery**: Boot into TWRP/Fastboot and reflash original vendor partition

### Cannot push files to /vendor
This is expected on modern devices. You must:
1. Use factory firmware as base
2. Unpack → Modify → Repack → Flash via fastboot
3. Do NOT rely on `adb remount` or `adb push`

## References

- [AOSP Power Supply HAL](https://cs.android.com/android/platform/superproject/main/+/main:hardware/libhardware/include/hardware/power_supply.h)
- [AIDL HAL Documentation](https://source.android.com/docs/core/architecture/hidl)
- [Linux Power Supply Subsystem](https://www.kernel.org/doc/html/latest/power/power_supply_class.html)
- [Xiaomi MiCharge Documentation](docs/micharge-analysis.md)

## Author

Created as part of the Xiaomi device porting project.

## Disclaimer

This project is provided as-is for educational and development purposes. Use at your own risk on production devices.

---

**Last Updated**: 2026-05-02
