# MiCharge kernel uevent helper

## Why this helper exists

Xiaomi framework code listens for:

```text
POWER_SUPPLY_QUICK_CHARGE_TYPE
```

through `UEventObserver`. On Xiaomi kernels this is produced by the charger
driver through `power_supply_changed()`. On non-Xiaomi devices the generic
charger driver usually has no `quick_charge_type` power-supply property, so
replug events do not make SystemUI receive a new quick-charge broadcast.

This helper keeps framework untouched:

```text
micharge HAL
  -> calculates quick_charge_type from usb current_max * voltage_max
  -> writes /sys/kernel/micharge_uevent/quick_charge_type
  -> kernel helper emits POWER_SUPPLY_QUICK_CHARGE_TYPE
  -> MiuiBatteryServiceImpl broadcasts ACTION_QUICK_CHARGE_TYPE
  -> SystemUI updates icon/animation
```

## Current quick-charge mapping

The shim now defaults to the Xiaomi-style 4-level mapping:

```text
0: normal, < 10W
1: fast,   >= 10W
2: flash,  >= 20W
3: turbo,  >= 30W
4: super,  >= 50W
```

Runtime overrides:

```text
persist.vendor.micharge.fast_w
persist.vendor.micharge.flash_w
persist.vendor.micharge.turbo_w
persist.vendor.micharge.super_w
persist.vendor.micharge.type_fast
persist.vendor.micharge.type_flash
persist.vendor.micharge.type_turbo
persist.vendor.micharge.type_super
persist.vendor.micharge.power_tiers_w
persist.vendor.micharge.force_power_w
persist.vendor.micharge.uevent_pulse_zero
```

`POWER_SUPPLY_POWER_MAX` is emitted as the advertised wattage tier, not the raw realtime
`current_max * voltage_max` value. The raw wattage is rounded up to the next tier. The default tier
list is:

```text
5,10,15,18,22,27,33,50,67,90,120
```

Positive quick-charge uevents use a fixed delayed snapshot:

```text
quick state: wait 1500 ms, reread current USB power nodes, then emit
attach reset guard: 500 ms
```

When a quick state first appears, the HAL waits 1500 ms and then reads the current
`/sys/class/power_supply/usb/current_max` and `voltage_max` values. The value at that moment decides
the quick-charge type and advertised power tier. `typec_portx_dpdm_attach=1` still marks a new attach
epoch so a replug with the same quick type is not hidden by the cached previous value.

`smart_chg` is handled in the shim:

```text
0x10     -> normal full charge
0x500011 -> stop/limit charging at 80%
```

For the 80% mode, the HAL reads `/sys/class/power_supply/battery/capacity` and requests init to
write `/sys/class/qcom-battery/battery_charging_enabled=0` when capacity reaches 80. This stops
battery charging while leaving input/bypass power alone. When normal full charge is selected again,
the HAL requests `battery_charging_enabled=1`.

The selected mode is persisted in:

```text
persist.vendor.micharge.smart_chg=normal|limit80
```

The HAL does not write the root-owned sysfs node directly. It emits property pulses for init:

```rc
on property:vendor.micharge.charge_pause_seq=*
    write /sys/class/qcom-battery/battery_charging_enabled 0

on property:vendor.micharge.charge_resume_seq=*
    write /sys/class/qcom-battery/battery_charging_enabled 1
```

The helper bridge can be disabled with:

```text
persist.vendor.micharge.uevent_helper=0
```

## Files

User-space HAL output:

```text
shim/micharge/out/vendor.xiaomi.hardware.micharge-service
```

Kernel helper source:

```text
kernel/micharge_uevent_helper/micharge_uevent.c
kernel/micharge_uevent_helper/Makefile
kernel/micharge_uevent_helper/build.ps1
kernel/micharge_uevent_helper/init.micharge_uevent.rc
```

## Building the kernel module

NDK cannot build `.ko` files by itself. You need the exact target kernel build
tree or GKI external module headers matching the booted kernel.

The Xiaomi sample module reports:

```text
6.12.23-android16-5-gd8653a69a3aa-mi-4k SMP preempt mod_unload modversions aarch64
```

Build example:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\kernel\micharge_uevent_helper\build.ps1 -KernelDir C:\path\to\target\kernel\out
```

Expected output:

```text
kernel/micharge_uevent_helper/out/micharge_uevent.ko
```

## Install locations

Put the helper module in `vendor_dlkm`:

```text
/vendor_dlkm/lib/modules/micharge_uevent.ko
```

Put the init rc in vendor:

```text
/vendor/etc/init/init.micharge_uevent.rc
```

Put the HAL service in vendor:

```text
/vendor/bin/hw/vendor.xiaomi.hardware.micharge-service
```

Keep the existing micharge vintf manifest and service rc:

```text
/vendor/etc/vintf/manifest/vendor.xiaomi.hardware.micharge.xml
/vendor/etc/init/vendor.xiaomi.hardware.micharge-service.rc
```

If your ROM uses `modules.load`, add:

```text
micharge_uevent.ko
```

to the vendor_dlkm module load list instead of using `insmod` from rc, but keep
the rc permission block for `/sys/kernel/micharge_uevent/*`.

## Runtime checks

After boot:

```bash
adb shell ls -l /sys/kernel/micharge_uevent
adb shell cat /sys/kernel/micharge_uevent/status
adb shell cat /sys/class/power_supply/usb/current_max
adb shell cat /sys/class/power_supply/usb/voltage_max
```

Manual uevent test:

```bash
adb shell 'echo 62 > /sys/kernel/micharge_uevent/power_max'
adb shell 'echo 1 > /sys/kernel/micharge_uevent/online'
adb shell 'echo PD_PPS > /sys/kernel/micharge_uevent/usb_type'
adb shell 'echo 4 > /sys/kernel/micharge_uevent/quick_charge_type'
```

Logcat:

```bash
adb logcat -b all -v threadtime | grep -iE 'MiChargeShim|MiuiBatteryServiceImpl|ACTION_QUICK_CHARGE_TYPE|quick_charge_type'
```

Kernel log:

```bash
adb shell dmesg | grep -i micharge_uevent
```
