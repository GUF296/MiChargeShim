# MiCharge shim test plan

## Build on Windows

```powershell
.\shim\micharge\build.ps1
```

If PowerShell blocks local scripts:

```powershell
powershell -ExecutionPolicy Bypass -File .\shim\micharge\build.ps1
```

If your NDK is elsewhere:

```powershell
.\shim\micharge\build.ps1 -Ndk C:\Users\GUF296\Documents\workspace\android-ndk-r27d
```

Output:

```text
shim/micharge/out/vendor.xiaomi.hardware.micharge-service
```

The current build statically links the C++ runtime, so you do not need to push
`libc++_shared.so`.

## Files to push

Keep the original VINTF XML and AIDL NDK library on the device. Replace only the service binary for
the first test:

```bash
adb root
adb remount
adb shell mv /vendor/bin/hw/vendor.xiaomi.hardware.micharge-service /vendor/bin/hw/vendor.xiaomi.hardware.micharge-service.orig
adb push shim/micharge/out/vendor.xiaomi.hardware.micharge-service /vendor/bin/hw/vendor.xiaomi.hardware.micharge-service
adb shell chmod 0755 /vendor/bin/hw/vendor.xiaomi.hardware.micharge-service
adb shell chown root:shell /vendor/bin/hw/vendor.xiaomi.hardware.micharge-service
adb reboot
```

On some builds the original owner/group may differ. Matching the original mode/owner is fine.

## Logs to capture

```bash
adb logcat -b all -v threadtime -s MiChargeShim '*:S'
```

Useful broader capture:

```bash
adb logcat -b all -v threadtime | grep -iE 'MiChargeShim|micharge|quick_charge|fast_charge|power_max'
```

SystemUI-side quick-charge broadcast capture:

```bash
adb logcat -b all -v threadtime | grep -iE 'MiChargeShim|MiuiChargeManager|BatteryController|MiuiChargeIconView|ACTION_QUICK_CHARGE_TYPE|ACTION_HVDCP_TYPE|quick_charge_type|chargeSpeed|BatteryService'
```

On Windows PowerShell/cmd, use `findstr` instead of `grep`:

```powershell
adb logcat -b all -v threadtime | findstr /i "MiChargeShim MiuiChargeManager BatteryController MiuiChargeIconView ACTION_QUICK_CHARGE_TYPE ACTION_HVDCP_TYPE quick_charge_type chargeSpeed BatteryService"
```

Manual SystemUI receiver test:

```bash
adb shell am broadcast --receiver-registered-only -a miui.intent.action.ACTION_QUICK_CHARGE_TYPE --ei miui.intent.extra.quick_charge_type 2 --ei miui.intent.extra.POWER_MAX 25 --ei miui.intent.extra.CAR_CHARGE 0
```

If that immediately changes the icon or logs `chargeSpeed: 2`, the HAL values are good and the
missing piece is the framework service that should send `ACTION_QUICK_CHARGE_TYPE` after replug.

## Basic service checks

```bash
adb shell service list | grep -i micharge
adb shell dumpsys vendor.xiaomi.hardware.micharge.IMiCharge/default
```

If `dumpsys` does not print anything useful, it is still enough if `service list` shows the binder
and logcat shows `registered vendor.xiaomi.hardware.micharge.IMiCharge/default`.

## Simulated authentication checks

This shim simulates the useful `batterysecret` outputs when Xiaomi `xm_power` nodes do not exist.
The default test mode is enabled:

```bash
adb shell setprop persist.vendor.micharge.sim_auth 1
adb shell setprop persist.vendor.micharge.battery_auth 1
adb shell setprop persist.vendor.micharge.battery_type_raw 0
```

Expected `MiChargeShim` lines while plugged into a fast/high-power charger:

```text
getPdAuthentication() -> "1" source=sim_batterysecret pd_auth ...
getBatteryAuthentic() -> "1" source=sim_batterysecret battery_auth
getMiChargePath(key="verified") -> "1" source=sim_batterysecret pd_auth ...
getMiChargePath(key="is_pd_adapter") -> "1" source=sim_batterysecret is_pd_adapter ...
getQuickChargeType() -> "1" / "2" / "3"
getChargingPowerMax() -> "<watts>"
```

## Charging-state probe commands

Run these while unplugged, normal charging, fast charging, and super/high-power charging:

```bash
adb shell 'for f in \
/sys/class/xm_power/charger/charger_common/quick_charge_type \
/sys/class/xm_power/typec/strategy_pd_auth/verified \
/sys/class/xm_power/typec/strategy_pd_auth/current_state \
/sys/class/xm_power/typec/strategy_pd_auth/verify_process \
/sys/class/xm_power/typec/strategy_pd_auth/is_pd_adapter \
/sys/class/xm_power/fuelgauge/strategy_fg/authentic \
/sys/class/xm_power/fuelgauge/strategy_fg/slave_authentic \
/sys/class/xm_power/fuelgauge/strategy_fg/fast_charge \
/sys/class/xm_power/charger/charger_common/power_max \
/sys/class/xm_power/charger/charger_common/real_type \
/sys/class/qcom-battery/pd_verifed \
/sys/class/qcom-battery/verify_process \
/sys/class/power_supply/usb/current_max \
/sys/class/power_supply/usb/voltage_max \
/sys/class/power_supply/usb/real_type \
/sys/class/power_supply/usb/type \
/sys/class/power_supply/usb/current_now \
/sys/class/power_supply/usb/voltage_now \
/sys/class/power_supply/battery/current_now \
/sys/class/power_supply/battery/voltage_now; do [ -e "$f" ] && echo "$f=$(cat "$f")"; done'
```

Then trigger the UI paths:

- open Settings battery page
- plug/unplug charger
- switch between known normal and fast chargers
- observe lockscreen/battery icon text if applicable

Record the `MiChargeShim` method names, arguments, return values, and sysfs node values for each
charger state.

Batterysecret-focused log capture on a Xiaomi stock kernel:

```bash
adb logcat -b all -v threadtime | grep -iE 'batterysecret|pd-auth|verify_pd|strategy_pd_auth|request_vdm|pd_verif|authentic|micharge'
```

On Windows PowerShell/cmd:

```powershell
adb logcat -b all -v threadtime | findstr /i "batterysecret pd-auth verify_pd strategy_pd_auth request_vdm pd_verif authentic micharge"
```

## If more files are needed

If calls do not arrive or the framework expects another interface, pull these from the stock ROM:

```bash
adb pull /vendor/etc/init/vendor.xiaomi.hardware.micharge-service.rc
adb pull /vendor/etc/vintf/manifest/vendor.xiaomi.hardware.micharge.xml
adb pull /vendor/bin/hw/vendor.xiaomi.hardware.micharge-service
adb pull /vendor/lib64/vendor.xiaomi.hardware.micharge-V2-ndk.so
adb shell 'find /vendor /system_ext /product /system -iname "*micharge*" -o -iname "*MiCharge*"'
```

If the UI still does not query the HAL, capture framework-side references:

```bash
adb logcat -b all -v threadtime | grep -iE 'MiCharge|micharge|Battery|Charge|quick'
```
