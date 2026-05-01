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

### Deploy HAL Shim

```bash
adb root
adb remount
adb shell mv /vendor/bin/hw/vendor.xiaomi.hardware.micharge-service \
              /vendor/bin/hw/vendor.xiaomi.hardware.micharge-service.bak
adb push shim/micharge/out/vendor.xiaomi.hardware.micharge-service \
        /vendor/bin/hw/vendor.xiaomi.hardware.micharge-service
adb shell chmod 755 /vendor/bin/hw/vendor.xiaomi.hardware.micharge-service
adb reboot
```

### Install Kernel Module

Place the compiled `.ko` file on the target device:

```bash
adb push kernel/micharge_uevent_helper/micharge_uevent.ko \
        /vendor_dlkm/lib/modules/
adb push kernel/micharge_uevent_helper/init.micharge_uevent.rc \
        /vendor/etc/init/
adb reboot
```

Alternatively, if your device uses `modules.load`:
```bash
adb shell "echo micharge_uevent.ko >> /vendor_dlkm/etc/modules.load"
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

### HAL service crashes
- Check logcat for specific error messages
- Verify all required sysfs nodes exist on device
- Ensure proper permissions on `/sys/kernel/micharge_uevent/`

### No uevents received
- Confirm kernel module is loaded: `lsmod | grep micharge`
- Monitor with: `adb shell 'cat /proc/sys/kernel/uevent_seqnum'`
- Check sysfs write permissions

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
