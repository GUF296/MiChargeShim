# MiCharge HAL notes

## Original files

Original vendor artifacts are stored in `samples/original/`:

- `vendor.xiaomi.hardware.micharge-service`
- `vendor.xiaomi.hardware.micharge-service.rc`
- `vendor.xiaomi.hardware.micharge-V2-ndk.so`
- `vendor.xiaomi.hardware.micharge.xml`

## Service shape

`vendor.xiaomi.hardware.micharge-service.rc` declares one AIDL HAL service:

```rc
service micharge-hal /vendor/bin/hw/vendor.xiaomi.hardware.micharge-service
    interface aidl vendor.xiaomi.hardware.micharge.IMiCharge/default
    class hal
    user system
    group system
```

The VINTF fragment declares:

```xml
<hal format="aidl">
    <name>vendor.xiaomi.hardware.micharge</name>
    <version>2</version>
    <fqname>IMiCharge/default</fqname>
</hal>
```

The service links directly against `vendor.xiaomi.hardware.micharge-V2-ndk.so` and imports
`BnMiCharge`; there is no obvious `micharge.default.so` legacy backend to replace. The practical
first shim is therefore a replacement for `/vendor/bin/hw/vendor.xiaomi.hardware.micharge-service`.

The original service reads many nodes directly, including:

- `/sys/class/xm_power/charger/charger_common/quick_charge_type`
- `/sys/class/xm_power/typec/strategy_pd_auth/verified`
- `/sys/class/xm_power/fuelgauge/strategy_fg/fast_charge`
- `/sys/class/xm_power/charger/charger_common/power_max`
- `/sys/class/xm_power/charger/charger_common/real_type`
- `/sys/class/xm_power/fuelgauge/strategy_fg/authentic`
- `/sys/class/xm_power/fuelgauge/strategy_fg/slave_authentic`
- `/sys/class/power_supply/usb/current_now`
- `/sys/class/power_supply/usb/voltage_now`
- `/sys/class/power_supply/battery/current_now`
- `/sys/class/power_supply/battery/voltage_now`

The original binary also loads optional prediction libraries:

- `/vendor/lib64/libmnn_wrapper.so`
- `/vendor/lib64/libchar_dura_predict.so`

Those are unrelated to the first logging shim.

## AIDL metadata

Descriptor:

```text
vendor.xiaomi.hardware.micharge.IMiCharge
```

Instance:

```text
vendor.xiaomi.hardware.micharge.IMiCharge/default
```

Version:

```text
2
```

Hash:

```text
f8886a701012d522ea9977256f52b3c47596963d
```

## Transaction map

Recovered from `vendor.xiaomi.hardware.micharge-V2-ndk.so`:

| Code | Method | Shape |
| ---: | --- | --- |
| 1 | `getBatteryAuthentic` | `() -> String` |
| 2 | `getBatteryCapacity` | `() -> String` |
| 3 | `getBatteryChargeFull` | `() -> String` |
| 4 | `getBatteryChargeType` | `() -> String` |
| 5 | `getBatteryCycleCount` | `() -> String` |
| 6 | `getBatteryIbat` | `() -> String` |
| 7 | `getBatteryResistance` | `() -> String` |
| 8 | `getBatterySoh` | `() -> String` |
| 9 | `getBatteryTbat` | `() -> String` |
| 10 | `getBatteryThermaLevel` | `() -> String` |
| 11 | `getBatteryVbat` | `() -> String` |
| 12 | `getBtTransferStartState` | `() -> String` |
| 13 | `getCarChargingType` | `() -> String` |
| 14 | `getChargingPowerMax` | `() -> String` |
| 15 | `getCoolModeState` | `() -> String` |
| 16 | `getFastChargeModeStatus` | `() -> String` |
| 17 | `getInputSuspendState` | `() -> String` |
| 18 | `getMiChargePath` | `(String path) -> String` |
| 19 | `getNightChargingState` | `() -> String` |
| 20 | `getPSValue` | `() -> String` |
| 21 | `getPdApdoMax` | `() -> String` |
| 22 | `getPdAuthentication` | `() -> String` |
| 23 | `getQuickChargeType` | `() -> String` |
| 24 | `getSBState` | `() -> String` |
| 25 | `getSocDecimal` | `() -> String` |
| 26 | `getSocDecimalRate` | `() -> String` |
| 27 | `getTxAdapt` | `() -> String` |
| 28 | `getUsbCurrent` | `() -> String` |
| 29 | `getUsbVoltage` | `() -> String` |
| 30 | `getWirelessChargingStatus` | `() -> String` |
| 31 | `getWirelessFwStatus` | `() -> String` |
| 32 | `getWirelessReverseStatus` | `() -> String` |
| 33 | `isBatteryLifeFunctionSupported` | `() -> bool` |
| 34 | `isDPConnected` | `() -> bool` |
| 35 | `isFunctionSupported` | `(String name) -> bool` |
| 36 | `isUSB32` | `() -> bool` |
| 37 | `isWirelessChargingSupported` | `() -> bool` |
| 38 | `isWiressFwUpdateSupported` | `() -> bool` |
| 39 | `setBtState` | `(String value) -> int` |
| 40 | `setBtTransferStartState` | `(String value) -> int` |
| 41 | `setCoolModeState` | `(String value) -> int` |
| 42 | `setInputSuspendState` | `(String value) -> int` |
| 43 | `setMiChargePath` | `(String path, String value) -> int` |
| 44 | `setNightChargingState` | `(String value) -> int` |
| 45 | `setRxCr` | `(String value) -> int` |
| 46 | `setSBState` | `(String value) -> int` |
| 47 | `setSmCountReset` | `(String value) -> int` |
| 48 | `setUpdateWirelessFw` | `(String value) -> int` |
| 49 | `setWirelessChargingEnabled` | `(bool enabled) -> int` |
| 50 | `setWlsTxSpeed` | `(String value) -> int` |
| 51 | `getTypeCCommonInfo` | `(String key) -> String` |
| 52 | `setTypeCCommonInfo` | `(String key, String value) -> int` |
| 53 | `getChargeCommonInfo` | `(String key) -> String` |
| 54 | `setChargeCommonInfo` | `(String key, String value) -> int` |
| 55 | `getBatteryCommonInfo` | `(String key) -> String` |
| 56 | `setBatteryCommonInfo` | `(String key, String value) -> int` |
| 16777214 | `getInterfaceHash` | `() -> String` |
| 16777215 | `getInterfaceVersion` | `() -> int` |

For fast-charge UI work, the first methods to watch are:

- `getQuickChargeType()`
- `getFastChargeModeStatus()`
- `getChargingPowerMax()`
- `getBatteryChargeType()`
- `getChargeCommonInfo("quick_charge_type" | "power_max" | "real_type" | ...)`
- `getMiChargePath(...)` if framework passes raw or aliased sysfs paths

## Log findings on non-Xiaomi target

The first captured log shows the Xiaomi framework reaches this HAL successfully. The important
charging UI calls are:

- `getQuickChargeType()` during battery/settings initialization.
- `getFastChargeModeStatus()` during the same polling path.
- `getChargingPowerMax()` when charging state is refreshed.
- `getBatteryChargeType()` repeatedly, roughly every 500 ms in the captured window.

Many battery-health keys are also queried through `getMiChargePath(...)`, for example
`charge_counter`, `charge_full_design`, `batt_sn`, `ui_soh`, and SOH learning keys. These are
secondary for fast-charge animation.

On a non-Xiaomi device, `/sys/class/xm_power/*` is expected to be missing. The shim now falls back
to generic USB power-supply negotiation data:

```text
/sys/class/power_supply/usb/current_max
/sys/class/power_supply/usb/voltage_max
```

The computed power is:

```text
power_mw = current_max_uA * voltage_max_uV / 1,000,000,000
```

The current experimental mapping is:

| Power | Returned quick charge type |
| ---: | ---: |
| `< 10 W` | `0` |
| `>= 10 W` | `1` |
| `>= 25 W` | `2` |
| `>= 50 W` | `3` |

These defaults are tunable at runtime:

```bash
adb shell setprop persist.vendor.micharge.fast_w 10
adb shell setprop persist.vendor.micharge.turbo_w 25
adb shell setprop persist.vendor.micharge.super_w 50
adb shell setprop persist.vendor.micharge.type_fast 1
adb shell setprop persist.vendor.micharge.type_turbo 2
adb shell setprop persist.vendor.micharge.type_super 3
```

Displayed/advertised power:

- Xiaomi framework broadcasts `miui.intent.extra.POWER_MAX` from `getChargingPowerMax()` without
  recalculating it.
- SystemUI stores that value as `BatteryStatus.maxChargingWattage` and displays it directly in the
  turbo view and strong toast.
- The shim therefore keeps raw USB power for detection/logging, but rounds the framework-visible
  `power_max` up to the next advertised wattage tier.

Default advertised tiers:

```text
5,10,15,18,22,27,33,50,67,90,120
```

Runtime tuning:

```bash
adb shell setprop persist.vendor.micharge.power_tiers_w "5,10,15,18,22,27,33,50,67,90,120"
adb shell setprop persist.vendor.micharge.force_power_w 0
```

For example, a raw 72 W USB estimate maps to 90 W. A raw 15 W estimate maps to 15 W, and
16 W maps to 18 W.

Set `force_power_w` to a non-zero value to force one advertised wattage while testing a charger.

Uevent behavior:

- Offline reads from `getQuickChargeType()`, `getChargingPowerMax()`,
  `getFastChargeModeStatus()`, and `getMiChargePath("typec_portx_dpdm_attach")` emit
  `POWER_SUPPLY_QUICK_CHARGE_TYPE=0`.
- Positive quick-charge events wait for a built-in 1500 ms delay, then reread the current USB power
  nodes and emit one `0 -> N` transition using whatever power is present at that moment.
- `getMiChargePath("typec_portx_dpdm_attach") == 1` marks a new attach epoch, so replugging the same
  adapter type is not hidden by Xiaomi framework's cached quick-charge value.
- The default positive event still writes a zero pulse before the positive value. This forces Xiaomi
  framework's cached quick-charge type to observe `0 -> N`.
- Same-state refreshes are disabled by default. Set `persist.vendor.micharge.uevent_refresh_ms` to
  a positive value if periodic re-emission is needed.

```bash
adb shell setprop persist.vendor.micharge.uevent_pulse_zero 1
adb shell setprop persist.vendor.micharge.uevent_refresh_ms 0
```

Smart charge behavior:

- `setMiChargePath("smart_chg", "0x10")` selects normal full charge.
- `setMiChargePath("smart_chg", "0x500011")` enables 80% protection.
- The selected mode is persisted in `persist.vendor.micharge.smart_chg`, so HAL/device restarts do
  not lose the user's choice.
- In 80% mode, the shim reads `/sys/class/power_supply/battery/capacity` and writes
  property pulses that make init write `/sys/class/qcom-battery/battery_charging_enabled=0` when
  capacity reaches 80. This pauses battery charging while keeping input/bypass power available.
  Switching back to normal asks init to write `battery_charging_enabled=1`.
- The capacity check is a lightweight poll: once 80% protection is selected, a helper thread checks
  every 30 seconds. HAL charge/quick-charge reads also reapply the setting with a 1 second throttle,
  which helps restore `battery_charging_enabled=0` after unplug/replug resets the node to `1`.

Mapped methods:

- `getQuickChargeType()` returns the quick-charge enum above.
- `getFastChargeModeStatus()` returns `1` when the enum is non-zero.
- `getChargingPowerMax()` returns the advertised wattage tier.
- `getChargeCommonInfo("quick_charge_type" | "fast_charge" | "power_max")` returns the same values.
- `getBatteryChargeType()` first tries `/sys/class/power_supply/usb/real_type` or `type`, then
  falls back to the quick-charge enum.

The log source string includes `current_ua`, `voltage_uv`, `power_mw`, and uevent logs include both
`power_w` and `raw_power_w` so the thresholds can be tuned from real charger samples.

## Replug behavior note

In the replug log, the shim still reports fast-charge data after the cable is reinserted:

```text
getQuickChargeType() -> "1"
getChargingPowerMax() -> "15"
getFastChargeModeStatus() -> "1"
```

So the missing fast icon is probably not caused by power estimation failing. Two suspicious return
shapes were visible:

- After replug, the high-frequency path mostly calls `getBatteryChargeType()` and
  `getTypeCCommonInfo("getTypecPortNum")`.
- `getBatteryChargeType()` returned the raw standard power-supply string `USB_PD`.
- `getTypeCCommonInfo("getTypecPortNum")` returned `0`.

For Xiaomi framework this may mean the replug state machine never receives a Xiaomi-style charge
type/Type-C-port signal. The shim now changes the defaults:

- `getBatteryChargeType()` returns the computed quick-charge enum (`0`/`1`/`2`/`3`) by default.
- Set `persist.vendor.micharge.battery_type_raw=1` to return raw `/sys/class/power_supply/usb/type`
  again.
- `getTypeCCommonInfo("getTypecPortNum")` returns `1` by default.
- Set `persist.vendor.micharge.typec_port_num` to override the Type-C port count.
- `getMiChargePath("typec_portx_dpdm_attach")` maps to `/sys/class/power_supply/usb/online` or
  `present`.
- USB power estimation is gated by `usb/online` or `usb/present`, so stale `current_max` and
  `voltage_max` values after unplug do not keep reporting fast charge.
- `isWirelessChargingSupported()` defaults to false unless wireless is currently online or
  `persist.vendor.micharge.wireless_supported=1` is set.

## SystemUI quick-charge path

`samples/original/com.android.systemui` shows that SystemUI does not call the `micharge` binder
directly for the status-bar quick-charge icon. It listens for broadcasts and caches their extras.

`com/miui/charge/MiuiChargeManager` registers for:

```text
android.intent.action.BATTERY_CHANGED
miui.intent.action.ACTION_QUICK_CHARGE_TYPE
```

For `ACTION_QUICK_CHARGE_TYPE`, it reads:

```text
miui.intent.extra.quick_charge_type
miui.intent.extra.POWER_MAX
miui.intent.extra.CAR_CHARGE
```

For wired charging, `ChargeUtils.getChargeSpeed(wireState, chargeType)` maps the broadcast
`quick_charge_type` as follows:

| wireState | quick_charge_type | chargeSpeed |
| ---: | ---: | ---: |
| `0xb` wired | `1` | `1` |
| `0xb` wired | `2` or `3` | `2` |
| `0xb` wired | `4` | `3` |
| other/unknown | any | `0` |

The tiny battery icon path has a parallel receiver in
`com/miui/clock/tiny/utils/BatteryController`. It also listens for:

```text
miui.intent.action.ACTION_QUICK_CHARGE_TYPE
miui.intent.action.ACTION_HVDCP_TYPE
```

It updates `mQuickCharging` only when `chargeSpeed >= 1`.

This explains the observed "fast only after boot, normal after replug" behavior. On unplug,
SystemUI clears its cached charge type. On replug, the HAL returns `2` or `3`, but the icon will
remain normal unless the framework broadcasts `ACTION_QUICK_CHARGE_TYPE` again with a non-zero
`miui.intent.extra.quick_charge_type`.

Changing `persist.vendor.micharge.*` only changes the HAL return values. It will not update
SystemUI by itself if the framework-side broadcast producer does not fire.

## Batterysecret and charger authentication

Additional original artifacts:

- `batterysecret`
- `init.batterysecret.rc`
- `vendor.xiaomi.hardware.batteryantiaging-service`
- `vendor.xiaomi.hardware.batteryantiaging-V1-ndk.so`
- `vendor.xiaomi.hardware.batteryantiaging-service.rc`
- `vendor.xiaomi.hardware.batteryantiaging.xml`
- `qti_battery_charger.ko`
- `qti_battery_debug.ko`
- `mca_business_battery_comp.ko`

`batterysecret` is not a binder HAL and does not expose a direct SystemUI status API. Its init
service is:

```rc
service batterysecret /vendor/bin/batterysecret
    class last_start
    user root
    group system system wakelock
    disabled
```

It is started on `sys.boot_completed=1` and in charger mode. The rc changes permissions for
charger authentication nodes:

```text
/sys/class/qcom-battery/pd_verifed
/sys/class/qcom-battery/request_vdm_cmd
/sys/class/qcom-battery/verify_process
/sys/class/xm_power/fuelgauge/strategy_fg/authentic
/sys/class/xm_power/fuelgauge/strategy_fg/verify_slave_flag
/sys/class/xm_power/fuelgauge/strategy_fg/slave_authentic
```

Strings in `batterysecret` show that it listens for USB power-supply uevents and performs Xiaomi
PD/PPS adapter and battery authentication through:

```text
/sys/class/xm_power/typec/strategy_pd_auth/current_state
/sys/class/xm_power/typec/strategy_pd_auth/data_role
/sys/class/xm_power/typec/strategy_pd_auth/is_pd_adapter
/sys/class/xm_power/typec/strategy_pd_auth/adapter_id
/sys/class/xm_power/typec/strategy_pd_auth/adapter_svid
/sys/class/xm_power/typec/strategy_pd_auth/request_vdm_cmd
/sys/class/xm_power/typec/strategy_pd_auth/verify_process
/sys/class/xm_power/typec/strategy_pd_auth/verified
/sys/class/xm_power/fuelgauge/strategy_fg/verify_digest
```

Observed log strings include:

```text
verify_pd_digest start
success to verify pd digest
failed to verify pd digest
pd-auth: %d
adapter is invaled
curr_state is disconnect but pd_verified is pass, need redo auth
```

This indicates the original chain is:

```text
USB/PD uevent
  -> batterysecret authenticates adapter/battery through strategy_pd_auth and fuelgauge nodes
  -> kernel/xm_power charger nodes expose verified/authentic/quick_charge_type/power_max
  -> micharge reads those nodes
  -> framework consumes micharge and produces higher-level charging UI state
```

For a non-Xiaomi device, the real `batterysecret` is unlikely to be useful unless the matching
`xm_power` and `strategy_pd_auth` kernel nodes exist. The portable shim should instead emulate the
important `micharge` outputs from generic power-supply data:

- `getPdAuthentication()` should probably return authenticated (`1`) while generic USB power
  estimation reports fast/high-power charging.
- `getBatteryAuthentic()` and `getMiChargePath("authentic")` can return `1` by default for a
  non-Xiaomi pack, unless testing shows Xiaomi framework treats this as unsafe.
- `getMiChargePath("verified")` and direct key/path reads for
  `strategy_pd_auth/verified` should mirror `getPdAuthentication()`.
- Keep `getQuickChargeType()`, `getBatteryChargeType()`, and `getChargingPowerMax()` based on
  `/sys/class/power_supply/usb/current_max` and `voltage_max`.

Current shim behavior:

- Real Xiaomi authentication nodes are still used first when present.
- Without Xiaomi nodes, `getBatteryAuthentic()` returns `1` by default. It can be overridden with
  `persist.vendor.micharge.battery_auth=0`.
- Without Xiaomi nodes, `getPdAuthentication()` returns `1` when generic USB power estimation is at
  least fast-charge level. It can be disabled with `persist.vendor.micharge.sim_auth=0`.
- `getMiChargePath("authentic")`, `getMiChargePath("slave_authentic")`,
  `getMiChargePath("verified")`, `getMiChargePath("pd_verifed")`,
  `getMiChargePath("verify_process")`, `getMiChargePath("is_pd_adapter")`, and
  `getMiChargePath("apdo_max")` are mapped to the same simulated batterysecret-style state.
- `is_pd_adapter` is also treated as true when generic USB type strings contain `PD`, `PPS`,
  `HVDCP`, `QC`, or `quick`, so a 5V/3A USB_PD source is not mistaken for a non-PD adapter.

`vendor.xiaomi.hardware.batteryantiaging` is a separate AIDL service for battery aging strategies.
Its interface contains:

```text
getBatteryantiagingInfo(int, String) -> String
sendBatteryantiagingCmd(int, String, String) -> int
registerCallback(IMessageCallback)
unregisterCallback(IMessageCallback)
```

The service depends on strategy libraries such as `libbaa_common.so`,
`libbaa_ChargeInfo.so`, and `libbaa_LowSohFvDown.so`, plus
`/data/vendor/batteryantiaging/BAA_battSpec.csv`. SystemUI smali does not reference
`IBatteryAntiAging`, so this service is more likely related to battery-health/anti-aging policy
than to the status-bar quick-charge icon.

## First logging shim

The shim in `shim/micharge/` directly implements the AIDL binder class with the same descriptor and
instance. It logs all calls with tag `MiChargeShim`.

Known read-only getters try to read matching `/sys/class/xm_power/*` or `/sys/class/power_supply/*`
nodes and fall back to `"0"` if unavailable. Setters only log input and return success.
