# micharge_uevent helper

This module bridges the portable `micharge` HAL shim to Xiaomi/HyperOS framework
code that listens for kernel power-supply uevents.

The HAL writes:

```text
/sys/kernel/micharge_uevent/power_max
/sys/kernel/micharge_uevent/online
/sys/kernel/micharge_uevent/usb_type
/sys/kernel/micharge_uevent/quick_charge_type
```

Writing `quick_charge_type` emits a `KOBJ_CHANGE` uevent. If the target kernel
has a `usb` power_supply, the uevent is emitted from that device kobject;
otherwise it is emitted from the helper kobject.

The important payload is:

```text
POWER_SUPPLY_NAME=usb
POWER_SUPPLY_QUICK_CHARGE_TYPE=0..4
POWER_SUPPLY_POWER_MAX=<watts>
POWER_SUPPLY_ONLINE=0|1
POWER_SUPPLY_USB_TYPE=<USB|USB_PD|PD_PPS|HVDCP...>
```

Build requires the exact target kernel build tree or GKI external module
headers. NDK alone is not enough for `.ko` output.

Example:

```powershell
.\kernel\micharge_uevent_helper\build.ps1 -KernelDir C:\path\to\kernel\out
```

Install on the target:

```text
/vendor_dlkm/lib/modules/micharge_uevent.ko
/vendor/etc/init/init.micharge_uevent.rc
```

If your ROM uses `modules.load` instead of init `insmod`, add this line to the
vendor_dlkm module load file instead:

```text
micharge_uevent.ko
```

Keep the rc permission block or make the `micharge` service run with a group
that can write the helper sysfs nodes.
