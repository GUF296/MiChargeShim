#include <android/binder_ibinder.h>
#include <android/binder_parcel.h>
#include <android/binder_parcel_utils.h>
#include <android/binder_status.h>
#include <android/log.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/system_properties.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <chrono>
#include <fstream>
#include <initializer_list>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr const char* kTag = "MiChargeShim";
constexpr const char* kDescriptor = "vendor.xiaomi.hardware.micharge.IMiCharge";
constexpr const char* kInstance = "vendor.xiaomi.hardware.micharge.IMiCharge/default";
constexpr int32_t kInterfaceVersion = 2;
constexpr const char* kInterfaceHash = "f8886a701012d522ea9977256f52b3c47596963d";
constexpr transaction_code_t kGetInterfaceHash = 16777214;
constexpr transaction_code_t kGetInterfaceVersion = 16777215;
constexpr const char* kUeventHelperDir = "/sys/kernel/micharge_uevent";
constexpr const char* kUeventHelperQuickChargeType =
        "/sys/kernel/micharge_uevent/quick_charge_type";
constexpr const char* kUeventHelperPowerMax = "/sys/kernel/micharge_uevent/power_max";
constexpr const char* kUeventHelperUsbType = "/sys/kernel/micharge_uevent/usb_type";
constexpr const char* kUeventHelperOnline = "/sys/kernel/micharge_uevent/online";
constexpr const char* kBatteryCapacityPath = "/sys/class/power_supply/battery/capacity";
constexpr int64_t kQuickChargeDelayMs = 1500;
constexpr int64_t kQuickChargeAttachResetMinMs = 500;
constexpr bool kQuickChargeAttachReset = true;
constexpr int64_t kSmartChargeStopCapacity = 80;
constexpr int64_t kSmartChargeApplyThrottleMs = 1000;
constexpr int64_t kSmartChargeMonitorIntervalMs = 30000;
constexpr const char* kSmartChargeNormalValue = "0x10";
constexpr const char* kSmartChargeLimit80Value = "0x500011";
constexpr const char* kSmartChargeModeProp = "persist.vendor.micharge.smart_chg";
constexpr const char* kSmartChargeModeNormal = "normal";
constexpr const char* kSmartChargeModeLimit80 = "limit80";
constexpr const char* kBatteryChargingEnabledProp =
        "vendor.micharge.battery_charging_enabled";
constexpr const char* kBatteryChargingPauseSeqProp = "vendor.micharge.charge_pause_seq";
constexpr const char* kBatteryChargingResumeSeqProp = "vendor.micharge.charge_resume_seq";

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, kTag, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, kTag, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, kTag, __VA_ARGS__)

using AddServiceFn = binder_status_t (*)(AIBinder*, const char*);
using SetThreadPoolFn = void (*)(uint32_t);
using JoinThreadPoolFn = void (*)();
using MarkVintfStabilityFn = void (*)(AIBinder*);

struct NodeAlias {
    const char* key;
    const char* path;
};

const NodeAlias kNodeAliases[] = {
        {"authentic", "/sys/class/xm_power/fuelgauge/strategy_fg/authentic"},
        {"slave_authentic", "/sys/class/xm_power/fuelgauge/strategy_fg/slave_authentic"},
        {"battery_num", "/sys/class/xm_power/fuelgauge/strategy_fg/battery_num"},
        {"batt_emerg", "/sys/class/xm_power/fuelgauge/strategy_fg/batt_emerg"},
        {"calc_rvalue", "/sys/class/xm_power/fuelgauge/strategy_fg/calc_rvalue"},
        {"enable_rollback", "/sys/class/xm_power/fuelgauge/strategy_fg/enable_rollback"},
        {"fast_charge", "/sys/class/xm_power/fuelgauge/strategy_fg/fast_charge"},
        {"soc_decimal", "/sys/class/xm_power/fuelgauge/strategy_fg/soc_decimal"},
        {"soc_decimal_rate", "/sys/class/xm_power/fuelgauge/strategy_fg/soc_decimal_rate"},
        {"audio_state", "/sys/class/xm_power/fuelgauge/strategy_fg/audio_state"},

        {"charge_enable", "/sys/class/xm_power/charger/charge_interface/charge_enable"},
        {"input_suspend", "/sys/class/xm_power/charger/charge_interface/input_suspend"},
        {"iin_limit", "/sys/class/xm_power/charger/charge_interface/iin_limit"},
        {"shipmode_count_reset",
         "/sys/class/xm_power/charger/charge_interface/shipmode_count_reset"},

        {"quick_charge_type", "/sys/class/xm_power/charger/charger_common/quick_charge_type"},
        {"real_type", "/sys/class/xm_power/charger/charger_common/real_type"},
        {"power_max", "/sys/class/xm_power/charger/charger_common/power_max"},
        {"reverse_quick_charge", "/sys/class/xm_power/charger/charger_common/reverse_quick_charge"},
        {"revchg_bcl", "/sys/class/xm_power/charger/charger_common/revchg_bcl"},
        {"handle_state", "/sys/class/xm_power/charger/charger_common/handle_state"},
        {"stop_handle_charge", "/sys/class/xm_power/charger/charger_common/stop_handle_charge"},
        {"plate_shock", "/sys/class/xm_power/charger/charger_common/plate_shock"},

        {"smart_night", "/sys/class/xm_power/charger/smart_charge/smart_night"},
        {"smart_batt", "/sys/class/xm_power/charger/smart_charge/smart_batt"},
        {"smart_chg", "/sys/class/xm_power/charger/smart_charge/smart_chg"},

        {"wired_ctrl_limit", "/sys/class/xm_power/charger/charger_thermal/wired_ctrl_limit"},
        {"wireless_ctrl_limit", "/sys/class/xm_power/charger/charger_thermal/wireless_ctrl_limit"},
        {"wls_quick_chg_control_limit",
         "/sys/class/xm_power/charger/charger_thermal/wls_quick_chg_control_limit"},

        {"reverse_chg_mode", "/sys/class/xm_power/charger/wls_rev_charge/reverse_chg_mode"},
        {"reverse_chg_state", "/sys/class/xm_power/charger/wls_rev_charge/reverse_chg_state"},
        {"wireless_chip_fw", "/sys/class/xm_power/charger/wls_rev_charge/wireless_chip_fw"},
        {"wls_fw_state", "/sys/class/xm_power/charger/wls_rev_charge/wls_fw_state"},

        {"bt_state", "/sys/class/xm_power/wireless_master/bt_state"},
        {"bt_transfer_start", "/sys/class/xm_power/wireless_master/bt_transfer_start"},
        {"rx_cr", "/sys/class/xm_power/wireless_master/rx_cr"},
        {"tx_adapter", "/sys/class/xm_power/wireless_master/tx_adapter"},
        {"wls_tx_speed", "/sys/class/xm_power/wireless_master/wls_tx_speed"},

        {"apdo_max", "/sys/class/xm_power/typec/apdo_max"},
        {"cc_toggle", "/sys/class/xm_power/typec/cc_toggle"},
        {"cid_status", "/sys/class/xm_power/typec/cid_status"},
        {"has_dp", "/sys/class/xm_power/typec/has_dp"},
        {"otg_ui_support", "/sys/class/xm_power/typec/otg_ui_support"},
        {"super_speed", "/sys/class/xm_power/typec/super_speed"},
        {"verified", "/sys/class/xm_power/typec/strategy_pd_auth/verified"},

        {"capacity", "/sys/class/power_supply/battery/capacity"},
        {"charge_counter", "/sys/class/power_supply/battery/charge_counter"},
        {"charge_full", "/sys/class/power_supply/battery/charge_full"},
        {"charge_full_design", "/sys/class/power_supply/battery/charge_full_design"},
        {"current_now", "/sys/class/power_supply/battery/current_now"},
        {"cycle_count", "/sys/class/power_supply/battery/cycle_count"},
        {"temp", "/sys/class/power_supply/battery/temp"},
        {"voltage_now", "/sys/class/power_supply/battery/voltage_now"},

        {"usb_current_max", "/sys/class/power_supply/usb/current_max"},
        {"usb_voltage_max", "/sys/class/power_supply/usb/voltage_max"},
};

std::string trim(std::string value) {
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(),
                                            [&](unsigned char ch) { return !is_space(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(),
                             [&](unsigned char ch) { return !is_space(ch); })
                        .base(),
                value.end());
    return value;
}

std::string lowerAscii(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

bool containsAny(std::string value, std::initializer_list<const char*> needles) {
    value = lowerAscii(std::move(value));
    for (const char* needle : needles) {
        if (needle != nullptr && value.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool pathExists(const char* path) {
    struct stat st {};
    return path != nullptr && stat(path, &st) == 0;
}

bool readFile(const char* path, std::string* out) {
    if (path == nullptr || out == nullptr) {
        return false;
    }

    std::ifstream file(path);
    if (!file.good()) {
        return false;
    }

    std::string value;
    std::getline(file, value, '\0');
    *out = trim(value);
    return true;
}

bool writeFile(const char* path, const std::string& value, int* savedErrno) {
    if (savedErrno != nullptr) {
        *savedErrno = 0;
    }
    if (path == nullptr) {
        if (savedErrno != nullptr) {
            *savedErrno = EINVAL;
        }
        return false;
    }

    const int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        if (savedErrno != nullptr) {
            *savedErrno = errno;
        }
        return false;
    }

    std::string payload = value;
    if (payload.empty() || payload.back() != '\n') {
        payload.push_back('\n');
    }

    const char* data = payload.c_str();
    size_t remaining = payload.size();
    while (remaining > 0) {
        const ssize_t written = write(fd, data, remaining);
        if (written < 0) {
            const int err = errno;
            close(fd);
            if (savedErrno != nullptr) {
                *savedErrno = err;
            }
            return false;
        }
        data += written;
        remaining -= static_cast<size_t>(written);
    }

    if (close(fd) != 0) {
        if (savedErrno != nullptr) {
            *savedErrno = errno;
        }
        return false;
    }
    return true;
}

bool readFirstValue(std::initializer_list<const char*> paths, std::string* out,
                    std::string* source) {
    for (const char* path : paths) {
        std::string value;
        if (readFile(path, &value)) {
            if (out != nullptr) {
                *out = value;
            }
            if (source != nullptr) {
                *source = path;
            }
            return true;
        }
    }
    return false;
}

std::string readFirst(std::initializer_list<const char*> paths, const char* fallback,
                      std::string* source) {
    for (const char* path : paths) {
        std::string value;
        if (readFile(path, &value)) {
            if (source != nullptr) {
                *source = path;
            }
            return value.empty() ? std::string(fallback) : value;
        }
    }
    if (source != nullptr) {
        *source = "fallback";
    }
    return fallback;
}

bool parseInt64(const std::string& raw, int64_t* out) {
    const std::string value = trim(raw);
    if (value.empty() || out == nullptr) {
        return false;
    }

    errno = 0;
    char* end = nullptr;
    const long long parsed = std::strtoll(value.c_str(), &end, 0);
    if (end == value.c_str() || errno == ERANGE) {
        return false;
    }

    *out = static_cast<int64_t>(parsed);
    return true;
}

int64_t readIntProperty(const char* name, int64_t fallback) {
    char value[PROP_VALUE_MAX] = {};
    if (__system_property_get(name, value) <= 0) {
        return fallback;
    }

    int64_t parsed = 0;
    if (!parseInt64(value, &parsed)) {
        LOGW("failed to parse property %s=\"%s\"", name, value);
        return fallback;
    }
    return parsed;
}

std::string readStringProperty(const char* name, const char* fallback) {
    char value[PROP_VALUE_MAX] = {};
    if (__system_property_get(name, value) <= 0 || value[0] == '\0') {
        return fallback == nullptr ? std::string() : std::string(fallback);
    }
    return value;
}

bool readInt64First(std::initializer_list<const char*> paths, int64_t* out, std::string* source) {
    std::string value;
    std::string valueSource;
    if (!readFirstValue(paths, &value, &valueSource)) {
        return false;
    }
    if (!parseInt64(value, out)) {
        LOGW("failed to parse integer from %s value=\"%s\"", valueSource.c_str(), value.c_str());
        return false;
    }
    if (source != nullptr) {
        *source = valueSource;
    }
    return true;
}

bool readOnlineState(std::initializer_list<const char*> paths, bool* online, std::string* source) {
    int64_t raw = 0;
    std::string rawSource;
    if (!readInt64First(paths, &raw, &rawSource)) {
        return false;
    }
    if (online != nullptr) {
        *online = raw > 0;
    }
    if (source != nullptr) {
        *source = rawSource + "=" + std::to_string(raw);
    }
    return true;
}

std::string readUsbOnlineValue(std::string* source) {
    bool online = false;
    std::string onlineSource;
    if (readOnlineState({"/sys/class/power_supply/usb/online",
                         "/sys/class/power_supply/usb/present"},
                        &online, &onlineSource)) {
        if (source != nullptr) {
            *source = onlineSource;
        }
        return online ? "1" : "0";
    }
    if (source != nullptr) {
        *source = "usb_online_missing";
    }
    return "0";
}

int64_t absoluteValue(int64_t value) {
    return value < 0 ? -value : value;
}

int64_t normalizeCurrentUa(int64_t raw) {
    const int64_t absRaw = absoluteValue(raw);
    if (absRaw > 0 && absRaw < 100000) {
        return raw * 1000;
    }
    return raw;
}

int64_t normalizeVoltageUv(int64_t raw) {
    const int64_t absRaw = absoluteValue(raw);
    if (absRaw > 0 && absRaw < 100000) {
        return raw * 1000;
    }
    return raw;
}

struct UsbPowerEstimate {
    bool valid = false;
    int64_t currentUa = 0;
    int64_t voltageUv = 0;
    int64_t powerMw = 0;
    std::string source;
};

int64_t elapsedRealtimeMs() {
    struct timespec ts {};
    if (clock_gettime(CLOCK_BOOTTIME, &ts) != 0 &&
        clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return static_cast<int64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

UsbPowerEstimate readUsbPowerEstimate() {
    UsbPowerEstimate estimate;
    bool usbOnline = false;
    std::string onlineSource;
    if (readOnlineState({"/sys/class/power_supply/usb/online",
                         "/sys/class/power_supply/usb/present"},
                        &usbOnline, &onlineSource) &&
        !usbOnline) {
        estimate.source = "usb_power offline " + onlineSource;
        return estimate;
    }

    int64_t currentRaw = 0;
    int64_t voltageRaw = 0;
    std::string currentSource;
    std::string voltageSource;

    const bool hasCurrent = readInt64First({"/sys/class/power_supply/usb/current_max",
                                            "/sys/class/power_supply/usb/input_current_limit"},
                                           &currentRaw, &currentSource);
    const bool hasVoltage = readInt64First({"/sys/class/power_supply/usb/voltage_max",
                                            "/sys/class/power_supply/usb/voltage_now"},
                                           &voltageRaw, &voltageSource);
    if (!hasCurrent || !hasVoltage) {
        estimate.source = "usb_power missing current_max_or_voltage_max";
        return estimate;
    }

    estimate.currentUa = normalizeCurrentUa(currentRaw);
    estimate.voltageUv = normalizeVoltageUv(voltageRaw);
    if (estimate.currentUa <= 0 || estimate.voltageUv <= 0) {
        estimate.source = "usb_power current_ua=" + std::to_string(estimate.currentUa) +
                          " voltage_uv=" + std::to_string(estimate.voltageUv);
        return estimate;
    }

    estimate.valid = true;
    estimate.powerMw = (estimate.currentUa * estimate.voltageUv + 500000000LL) / 1000000000LL;
    estimate.source = "usb_power " +
                      (onlineSource.empty() ? "" : "online(" + onlineSource + ") ") +
                      "current_ua=" + std::to_string(estimate.currentUa) + "(" +
                      currentSource + ") voltage_uv=" + std::to_string(estimate.voltageUv) +
                      "(" + voltageSource + ") power_mw=" + std::to_string(estimate.powerMw);
    return estimate;
}

int32_t quickChargeTypeFromPowerMw(int64_t powerMw) {
    const int64_t fastMw = readIntProperty("persist.vendor.micharge.fast_w", 10) * 1000;
    const int64_t flashMw = readIntProperty("persist.vendor.micharge.flash_w", 20) * 1000;
    const int64_t turboMw = readIntProperty("persist.vendor.micharge.turbo_w", 30) * 1000;
    const int64_t superMw = readIntProperty("persist.vendor.micharge.super_w", 50) * 1000;
    const int32_t fastType =
            static_cast<int32_t>(readIntProperty("persist.vendor.micharge.type_fast", 1));
    const int32_t flashType =
            static_cast<int32_t>(readIntProperty("persist.vendor.micharge.type_flash", 2));
    const int32_t turboType =
            static_cast<int32_t>(readIntProperty("persist.vendor.micharge.type_turbo", 3));
    const int32_t superType =
            static_cast<int32_t>(readIntProperty("persist.vendor.micharge.type_super", 4));

    if (powerMw >= superMw) {
        return superType;
    }
    if (powerMw >= turboMw) {
        return turboType;
    }
    if (powerMw >= flashMw) {
        return flashType;
    }
    if (powerMw >= fastMw) {
        return fastType;
    }
    return 0;
}

std::vector<int64_t> parsePowerTiers(const std::string& raw) {
    std::vector<int64_t> tiers;
    size_t start = 0;
    while (start <= raw.size()) {
        const size_t end = raw.find(',', start);
        const std::string token =
                trim(raw.substr(start, end == std::string::npos ? std::string::npos
                                                                : end - start));
        int64_t value = 0;
        if (parseInt64(token, &value) && value > 0) {
            tiers.push_back(value);
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    std::sort(tiers.begin(), tiers.end());
    tiers.erase(std::unique(tiers.begin(), tiers.end()), tiers.end());
    return tiers;
}

int64_t advertisedChargingPowerW(const UsbPowerEstimate& estimate) {
    if (!estimate.valid) {
        return 0;
    }

    const int64_t forcedPowerW = readIntProperty("persist.vendor.micharge.force_power_w", 0);
    if (forcedPowerW > 0) {
        return forcedPowerW;
    }

    const int64_t rawPowerW = (estimate.powerMw + 999) / 1000;
    if (rawPowerW <= 0) {
        return 0;
    }

    const std::string tiersProp = readStringProperty(
            "persist.vendor.micharge.power_tiers_w", "5,10,15,18,22,27,33,50,67,90,120");
    std::vector<int64_t> tiers = parsePowerTiers(tiersProp);
    if (tiers.empty()) {
        return rawPowerW;
    }

    for (const int64_t tier : tiers) {
        if (rawPowerW <= tier) {
            return tier;
        }
    }
    return rawPowerW;
}

int32_t parseInt32Or(const std::string& raw, int32_t fallback) {
    int64_t parsed = 0;
    if (!parseInt64(raw, &parsed)) {
        return fallback;
    }
    return static_cast<int32_t>(parsed);
}

std::string uppercaseChargeType(std::string value) {
    value = trim(std::move(value));
    for (char& ch : value) {
        if (ch == '-' || ch == ' ') {
            ch = '_';
        } else {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
        }
    }
    return value;
}

std::string synthesizeChargeType(const UsbPowerEstimate& estimate) {
    if (!estimate.valid) {
        return "USB";
    }

    const int32_t quickType = quickChargeTypeFromPowerMw(estimate.powerMw);
    if (estimate.voltageUv > 5500000) {
        return quickType >= 3 ? "PD_PPS" : "USB_PD";
    }
    if (quickType > 0) {
        return "USB_DCP";
    }
    return "USB";
}

std::string normalizeChargeType(std::string value, const UsbPowerEstimate& estimate) {
    value = uppercaseChargeType(std::move(value));
    if (value.empty() || value == "UNKNOWN") {
        return synthesizeChargeType(estimate);
    }
    if (value.find("PPS") != std::string::npos) {
        return "PD_PPS";
    }
    if (value == "USB_PD" || value == "PD" || value.find("USB_PD") != std::string::npos) {
        return "USB_PD";
    }
    if (value.find("HVDCP_3P5") != std::string::npos) {
        return "HVDCP_3P5";
    }
    if (value.find("HVDCP_3") != std::string::npos) {
        return "HVDCP_3";
    }
    if (value.find("HVDCP") != std::string::npos || value.find("QC") != std::string::npos) {
        return "HVDCP";
    }
    if (value == "DCP" || value == "USB_DCP") {
        return "USB_DCP";
    }
    if (value == "CDP" || value == "USB_CDP") {
        return "USB_CDP";
    }
    if (value == "SDP" || value == "USB_SDP" || value == "USB") {
        return "USB";
    }
    if (value == "FLOAT" || value == "USB_FLOAT") {
        return "USB_FLOAT";
    }
    return value;
}

std::string readUsbChargeTypeForEvent(const UsbPowerEstimate& estimate) {
    std::string value;
    std::string source;
    if (readFirstValue({"/sys/class/xm_power/charger/charger_common/real_type",
                        "/sys/class/power_supply/usb/real_type",
                        "/sys/class/power_supply/usb/type"},
                       &value, &source)) {
        return normalizeChargeType(value, estimate);
    }
    return synthesizeChargeType(estimate);
}

struct UeventWriteResult {
    bool wrotePower = false;
    bool wroteUsbType = false;
    bool wroteOnline = false;
    bool wroteQuick = false;
    int powerErrno = 0;
    int usbTypeErrno = 0;
    int onlineErrno = 0;
    int quickErrno = 0;
};

struct QuickChargeUeventState {
    int32_t quickType = 0;
    int64_t powerW = 0;
    int64_t rawPowerW = 0;
    bool online = false;
    std::string usbType;
    std::string reason;
};

std::mutex gQuickChargeUeventMutex;
int32_t gLastQuickType = -1;
int64_t gLastPowerW = -1;
bool gLastOnline = false;
int64_t gLastEmitMs = 0;
int64_t gPendingGeneration = 0;
bool gPendingActive = false;
bool gForceNextPositivePulse = false;
int64_t gLastAttachResetMs = 0;

std::mutex gSmartChargeMutex;
bool gSmartChargeLimit80Enabled = false;
bool gSmartChargeChargingPaused = false;
int64_t gSmartChargeLastApplyMs = 0;
int64_t gSmartChargeMonitorGeneration = 0;
bool gSmartChargeMonitorRunning = false;
int64_t gSmartChargePropertySeq = 0;

UeventWriteResult writeQuickChargeUeventNodes(int32_t quickType, int64_t powerW, bool online,
                                              const std::string& usbType) {
    UeventWriteResult result;
    result.wrotePower =
            writeFile(kUeventHelperPowerMax, std::to_string(powerW), &result.powerErrno);
    result.wroteUsbType = writeFile(kUeventHelperUsbType, usbType, &result.usbTypeErrno);
    result.wroteOnline =
            writeFile(kUeventHelperOnline, online ? "1" : "0", &result.onlineErrno);
    result.wroteQuick = writeFile(kUeventHelperQuickChargeType,
                                  std::to_string(std::max<int32_t>(quickType, 0)),
                                  &result.quickErrno);
    return result;
}

void logUeventWriteFailure(const UeventWriteResult& result) {
    LOGW("failed to write micharge uevent helper; power=%d(%d:%s) type=%d(%d:%s) "
         "online=%d(%d:%s) quick=%d(%d:%s)",
         result.wrotePower ? 1 : 0, result.powerErrno, strerror(result.powerErrno),
         result.wroteUsbType ? 1 : 0, result.usbTypeErrno, strerror(result.usbTypeErrno),
         result.wroteOnline ? 1 : 0, result.onlineErrno, strerror(result.onlineErrno),
         result.wroteQuick ? 1 : 0, result.quickErrno, strerror(result.quickErrno));
}

bool emitQuickChargeUeventState(const QuickChargeUeventState& state, bool pulseZero) {
    if (state.online && pulseZero) {
        const UeventWriteResult zeroResult =
                writeQuickChargeUeventNodes(0, 0, false, state.usbType);
        if (zeroResult.wroteQuick) {
            LOGI("emit POWER_SUPPLY_QUICK_CHARGE_TYPE=0 power_w=0 raw_power_w=0 online=0 "
                 "usb_type=%s reason=%s:pulse_zero",
                 state.usbType.c_str(), state.reason.c_str());
        } else {
            logUeventWriteFailure(zeroResult);
        }
    }

    const UeventWriteResult result = writeQuickChargeUeventNodes(
            state.quickType, state.powerW, state.online, state.usbType);
    if (result.wroteQuick) {
        LOGI("emit POWER_SUPPLY_QUICK_CHARGE_TYPE=%d power_w=%lld raw_power_w=%lld online=%d "
             "usb_type=%s reason=%s",
             state.quickType, static_cast<long long>(state.powerW),
             static_cast<long long>(state.rawPowerW), state.online ? 1 : 0,
             state.usbType.c_str(), state.reason.c_str());
        return true;
    }

    logUeventWriteFailure(result);
    return false;
}

void queueDelayedQuickChargeEmitLocked(const std::string& reason, int64_t delayMs) {
    if (delayMs <= 0 || gPendingActive) {
        return;
    }

    gPendingActive = true;
    const int64_t generation = ++gPendingGeneration;
    LOGI("queue delayed POWER_SUPPLY_QUICK_CHARGE_TYPE delay_ms=%lld reason=%s",
         static_cast<long long>(delayMs), reason.c_str());

    std::thread([generation, delayMs, reason]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

        const UsbPowerEstimate estimate = readUsbPowerEstimate();
        const bool online = estimate.valid;
        const int32_t quickType = online ? quickChargeTypeFromPowerMw(estimate.powerMw) : 0;
        const bool quickOnline = online && quickType > 0;
        QuickChargeUeventState emitState{std::max<int32_t>(quickType, 0),
                                         quickOnline ? advertisedChargingPowerW(estimate) : 0,
                                         quickOnline ? (estimate.powerMw + 500) / 1000 : 0,
                                         quickOnline,
                                         readUsbChargeTypeForEvent(estimate),
                                         reason + ":delayed"};

        std::lock_guard<std::mutex> threadLock(gQuickChargeUeventMutex);
        if (!gPendingActive || generation != gPendingGeneration) {
            return;
        }

        gPendingActive = false;
        const bool pulseZero =
                emitState.online &&
                readIntProperty("persist.vendor.micharge.uevent_pulse_zero", 1) != 0;
        if (emitQuickChargeUeventState(emitState, pulseZero)) {
            gLastQuickType = emitState.quickType;
            gLastPowerW = emitState.powerW;
            gLastOnline = emitState.online;
            gLastEmitMs = elapsedRealtimeMs();
            gForceNextPositivePulse = false;
        }
    }).detach();
}

void markQuickChargeAttachOnline(const char* reason) {
    if (!kQuickChargeAttachReset) {
        return;
    }

    const int64_t nowMs = elapsedRealtimeMs();
    const int64_t minMs = kQuickChargeAttachResetMinMs;
    std::lock_guard<std::mutex> lock(gQuickChargeUeventMutex);
    if (gForceNextPositivePulse) {
        return;
    }
    if (!gLastOnline && !gPendingActive) {
        return;
    }
    if (minMs > 0 && nowMs > 0 && gLastAttachResetMs > 0 &&
        nowMs - gLastAttachResetMs < minMs) {
        return;
    }

    gPendingActive = false;
    ++gPendingGeneration;
    gLastQuickType = 0;
    gLastPowerW = 0;
    gLastOnline = false;
    gLastEmitMs = 0;
    gForceNextPositivePulse = true;
    gLastAttachResetMs = nowMs;
    LOGI("mark quick charge attach epoch reason=%s", reason == nullptr ? "unknown" : reason);
}

std::string readSmartChargeMode(std::string* source) {
    std::lock_guard<std::mutex> lock(gSmartChargeMutex);
    if (source != nullptr) {
        *source = "shim smart_chg";
    }
    return gSmartChargeLimit80Enabled ? kSmartChargeLimit80Value : kSmartChargeNormalValue;
}

bool setPropertyValue(const char* name, const std::string& value) {
    if (__system_property_set(name, value.c_str()) == 0) {
        return true;
    }
    LOGW("failed to set property %s=%s", name, value.c_str());
    return false;
}

bool isSmartChargeLimit80Value(const std::string& value) {
    int64_t parsed = 0;
    return parseInt64(value, &parsed) && parsed == 0x500011;
}

bool isSmartChargeNormalValue(const std::string& value) {
    int64_t parsed = 0;
    return parseInt64(value, &parsed) && parsed == 0x10;
}

void requestBatteryChargingEnabledLocked(bool enabled, const char* reason) {
    const char* value = enabled ? "1" : "0";
    const char* pulseProp =
            enabled ? kBatteryChargingResumeSeqProp : kBatteryChargingPauseSeqProp;
    const int64_t nowMs = elapsedRealtimeMs();
    const std::string seq = std::to_string(nowMs > 0 ? nowMs : 0) + ":" +
                            std::to_string(++gSmartChargePropertySeq);

    setPropertyValue(kBatteryChargingEnabledProp, value);
    if (setPropertyValue(pulseProp, seq)) {
        const bool paused = !enabled;
        if (gSmartChargeChargingPaused != paused) {
            LOGI("smart_chg request battery_charging_enabled=%s pulse=%s reason=%s", value,
                 pulseProp, reason == nullptr ? "unknown" : reason);
        }
        gSmartChargeChargingPaused = paused;
    }
}

void applySmartChargeLimitLocked(const char* reason) {
    if (!gSmartChargeLimit80Enabled) {
        requestBatteryChargingEnabledLocked(true, reason);
        return;
    }

    int64_t capacity = 0;
    std::string capacitySource;
    if (!readInt64First({kBatteryCapacityPath}, &capacity, &capacitySource)) {
        LOGW("smart_chg cannot read capacity reason=%s", reason == nullptr ? "unknown" : reason);
        return;
    }

    if (capacity < kSmartChargeStopCapacity) {
        requestBatteryChargingEnabledLocked(true, reason);
        return;
    }

    if (!gSmartChargeChargingPaused) {
        LOGI("smart_chg limit charge capacity=%lld threshold=%lld reason=%s",
             static_cast<long long>(capacity), static_cast<long long>(kSmartChargeStopCapacity),
             reason == nullptr ? "unknown" : reason);
    }
    requestBatteryChargingEnabledLocked(false, reason);
}

void refreshSmartChargeLimit(const char* reason) {
    std::lock_guard<std::mutex> lock(gSmartChargeMutex);
    if (!gSmartChargeLimit80Enabled) {
        return;
    }

    const int64_t nowMs = elapsedRealtimeMs();
    if (nowMs > 0 && gSmartChargeLastApplyMs > 0 &&
        nowMs - gSmartChargeLastApplyMs < kSmartChargeApplyThrottleMs) {
        return;
    }

    applySmartChargeLimitLocked(reason);
    gSmartChargeLastApplyMs = nowMs;
}

void startSmartChargeMonitorLocked() {
    if (gSmartChargeMonitorRunning) {
        return;
    }

    gSmartChargeMonitorRunning = true;
    const int64_t generation = ++gSmartChargeMonitorGeneration;
    std::thread([generation]() {
        while (true) {
            {
                std::lock_guard<std::mutex> lock(gSmartChargeMutex);
                if (!gSmartChargeLimit80Enabled || generation != gSmartChargeMonitorGeneration) {
                    gSmartChargeMonitorRunning = false;
                    return;
                }
                applySmartChargeLimitLocked("monitor");
            }
            std::this_thread::sleep_for(
                    std::chrono::milliseconds(kSmartChargeMonitorIntervalMs));
        }
    }).detach();
}

void handleSmartChargeValue(const std::string& value, const char* reason) {
    std::lock_guard<std::mutex> lock(gSmartChargeMutex);
    if (isSmartChargeLimit80Value(value)) {
        gSmartChargeLimit80Enabled = true;
        gSmartChargeLastApplyMs = 0;
        setPropertyValue(kSmartChargeModeProp, kSmartChargeModeLimit80);
        LOGI("smart_chg mode=limit_80 value=%s caller=%d:%d", value.c_str(),
             AIBinder_getCallingUid(), AIBinder_getCallingPid());
        startSmartChargeMonitorLocked();
        applySmartChargeLimitLocked(reason);
        gSmartChargeLastApplyMs = elapsedRealtimeMs();
        return;
    }

    if (isSmartChargeNormalValue(value)) {
        gSmartChargeLimit80Enabled = false;
        ++gSmartChargeMonitorGeneration;
        gSmartChargeMonitorRunning = false;
        gSmartChargeLastApplyMs = 0;
        setPropertyValue(kSmartChargeModeProp, kSmartChargeModeNormal);
        LOGI("smart_chg mode=normal value=%s caller=%d:%d", value.c_str(),
             AIBinder_getCallingUid(), AIBinder_getCallingPid());
        requestBatteryChargingEnabledLocked(true, reason);
        return;
    }

    LOGI("smart_chg mode unchanged value=%s caller=%d:%d", value.c_str(),
         AIBinder_getCallingUid(), AIBinder_getCallingPid());
}

void initializeSmartChargeState() {
    const std::string mode = readStringProperty(kSmartChargeModeProp, kSmartChargeModeNormal);
    std::lock_guard<std::mutex> lock(gSmartChargeMutex);
    if (mode == kSmartChargeModeLimit80 || mode == kSmartChargeLimit80Value) {
        gSmartChargeLimit80Enabled = true;
        gSmartChargeLastApplyMs = 0;
        LOGI("smart_chg restore persisted mode=limit80");
        startSmartChargeMonitorLocked();
        applySmartChargeLimitLocked("boot_restore");
        gSmartChargeLastApplyMs = elapsedRealtimeMs();
        return;
    }

    gSmartChargeLimit80Enabled = false;
    LOGI("smart_chg restore persisted mode=normal");
    requestBatteryChargingEnabledLocked(true, "boot_restore");
}

void maybeEmitQuickChargeUevent(int32_t quickType, const UsbPowerEstimate& estimate,
                                const char* reason) {
    refreshSmartChargeLimit(reason == nullptr ? "micharge_poll" : reason);

    if (readIntProperty("persist.vendor.micharge.uevent_helper", 1) == 0) {
        return;
    }

    static bool missingLogged = false;
    if (!pathExists(kUeventHelperQuickChargeType)) {
        if (!missingLogged) {
            LOGI("micharge uevent helper is not present at %s; skip kernel uevent bridge",
                 kUeventHelperDir);
            missingLogged = true;
        }
        return;
    }

    const bool online = estimate.valid && quickType > 0;
    const int64_t rawPowerW = online ? (estimate.powerMw + 500) / 1000 : 0;
    const int64_t powerW = online ? advertisedChargingPowerW(estimate) : 0;
    const int64_t nowMs = elapsedRealtimeMs();
    const int64_t refreshMs = readIntProperty("persist.vendor.micharge.uevent_refresh_ms", 0);
    const int64_t delayMs = kQuickChargeDelayMs;

    std::lock_guard<std::mutex> lock(gQuickChargeUeventMutex);
    const std::string usbType = readUsbChargeTypeForEvent(estimate);
    QuickChargeUeventState state{std::max<int32_t>(quickType, 0),
                                 powerW,
                                 rawPowerW,
                                 online,
                                 usbType,
                                 reason == nullptr ? "unknown" : reason};

    if (!online) {
        gPendingActive = false;
        ++gPendingGeneration;
        gForceNextPositivePulse = false;
        const bool sameOffline =
                gLastQuickType == state.quickType && gLastPowerW == state.powerW &&
                gLastOnline == state.online;
        if (sameOffline && (refreshMs <= 0 || nowMs <= 0 || nowMs - gLastEmitMs < refreshMs)) {
            return;
        }
        if (emitQuickChargeUeventState(state, false)) {
            gLastQuickType = state.quickType;
            gLastPowerW = state.powerW;
            gLastOnline = state.online;
            gLastEmitMs = nowMs;
        }
        return;
    }

    const bool sameState =
            state.quickType == gLastQuickType && state.powerW == gLastPowerW &&
            state.online == gLastOnline;
    const bool firstPositive = !gLastOnline || gLastQuickType <= 0 || gForceNextPositivePulse;
    const bool typeChanged = gLastOnline && state.quickType != gLastQuickType;
    const bool powerChanged = gLastOnline && state.powerW != gLastPowerW;

    if (firstPositive || typeChanged || powerChanged) {
        queueDelayedQuickChargeEmitLocked(state.reason, delayMs);
        return;
    }

    if (sameState) {
        if (refreshMs <= 0 || nowMs <= 0 || nowMs - gLastEmitMs < refreshMs) {
            return;
        }
    }

    const bool pulseZero =
            readIntProperty("persist.vendor.micharge.uevent_pulse_zero", 1) != 0 &&
            (!gLastOnline || gLastQuickType <= 0);
    if (emitQuickChargeUeventState(state, pulseZero)) {
        gLastQuickType = state.quickType;
        gLastPowerW = state.powerW;
        gLastOnline = state.online;
        gLastEmitMs = nowMs;
    }
}

std::string readQuickChargeType(std::string* source) {
    std::string originalSource;
    std::string originalValue;
    if (readFirstValue({"/sys/class/xm_power/charger/charger_common/quick_charge_type"},
                       &originalValue, &originalSource)) {
        if (source != nullptr) {
            *source = originalSource;
        }
        const std::string value = originalValue.empty() ? "0" : originalValue;
        maybeEmitQuickChargeUevent(parseInt32Or(value, 0), readUsbPowerEstimate(),
                                   "getQuickChargeType:xm_power");
        return value;
    }

    const UsbPowerEstimate estimate = readUsbPowerEstimate();
    if (source != nullptr) {
        *source = estimate.source;
    }
    if (!estimate.valid) {
        maybeEmitQuickChargeUevent(0, estimate, "getQuickChargeType:offline");
        return "0";
    }
    const int32_t quickType = quickChargeTypeFromPowerMw(estimate.powerMw);
    maybeEmitQuickChargeUevent(quickType, estimate, "getQuickChargeType:usb_power");
    return std::to_string(quickType);
}

std::string readBatteryAuthentic(std::string* source) {
    std::string originalSource;
    std::string originalValue;
    if (readFirstValue({"/sys/class/xm_power/fuelgauge/strategy_fg/authentic",
                        "/sys/class/xm_power/fuelgauge/strategy_fg/slave_authentic"},
                       &originalValue, &originalSource)) {
        if (source != nullptr) {
            *source = originalSource;
        }
        return originalValue.empty() ? "0" : originalValue;
    }

    const bool authentic = readIntProperty("persist.vendor.micharge.battery_auth", 1) != 0;
    if (source != nullptr) {
        *source = "sim_batterysecret battery_auth";
    }
    return authentic ? "1" : "0";
}

std::string readPdAuthentication(std::string* source) {
    std::string originalSource;
    std::string originalValue;
    if (readFirstValue({"/sys/class/xm_power/typec/strategy_pd_auth/verified",
                        "/sys/class/qcom-battery/pd_verifed",
                        "/sys/class/qcom-battery/pd_verified"},
                       &originalValue, &originalSource)) {
        if (source != nullptr) {
            *source = originalSource;
        }
        return originalValue.empty() ? "0" : originalValue;
    }

    if (readIntProperty("persist.vendor.micharge.sim_auth", 1) == 0) {
        if (source != nullptr) {
            *source = "sim_batterysecret disabled";
        }
        return "0";
    }

    const UsbPowerEstimate estimate = readUsbPowerEstimate();
    if (source != nullptr) {
        *source = "sim_batterysecret pd_auth " + estimate.source;
    }
    if (!estimate.valid) {
        return "0";
    }
    return quickChargeTypeFromPowerMw(estimate.powerMw) > 0 ? "1" : "0";
}

std::string readPdVerifyProcess(std::string* source) {
    std::string originalSource;
    std::string originalValue;
    if (readFirstValue({"/sys/class/xm_power/typec/strategy_pd_auth/verify_process",
                        "/sys/class/qcom-battery/verify_process"},
                       &originalValue, &originalSource)) {
        if (source != nullptr) {
            *source = originalSource;
        }
        return originalValue.empty() ? "0" : originalValue;
    }

    std::string authSource;
    const std::string auth = readPdAuthentication(&authSource);
    if (source != nullptr) {
        *source = "sim_batterysecret verify_process " + authSource;
    }
    return auth == "0" ? "0" : "2";
}

std::string readIsPdAdapter(std::string* source) {
    std::string originalSource;
    std::string originalValue;
    if (readFirstValue({"/sys/class/xm_power/typec/strategy_pd_auth/is_pd_adapter"},
                       &originalValue, &originalSource)) {
        if (source != nullptr) {
            *source = originalSource;
        }
        return originalValue.empty() ? "0" : originalValue;
    }

    std::string typeValue;
    std::string typeSource;
    if (readFirstValue({"/sys/class/power_supply/usb/real_type",
                        "/sys/class/power_supply/usb/type"},
                       &typeValue, &typeSource) &&
        containsAny(typeValue, {"pd", "pps", "hvdcp", "qc", "quick"})) {
        if (source != nullptr) {
            *source = "sim_batterysecret is_pd_adapter " + typeSource + "=" + typeValue;
        }
        return "1";
    }

    const UsbPowerEstimate estimate = readUsbPowerEstimate();
    if (source != nullptr) {
        *source = "sim_batterysecret is_pd_adapter " + estimate.source;
    }
    if (!estimate.valid) {
        return "0";
    }
    return estimate.voltageUv > 5500000 || quickChargeTypeFromPowerMw(estimate.powerMw) >= 2 ? "1"
                                                                                              : "0";
}

std::string readPdApdoMax(std::string* source) {
    std::string originalSource;
    std::string originalValue;
    if (readFirstValue({"/sys/class/xm_power/typec/apdo_max"}, &originalValue, &originalSource)) {
        if (source != nullptr) {
            *source = originalSource;
        }
        return originalValue.empty() ? "0" : originalValue;
    }

    const UsbPowerEstimate estimate = readUsbPowerEstimate();
    if (source != nullptr) {
        *source = "sim_batterysecret apdo_max " + estimate.source;
    }
    if (!estimate.valid) {
        return "0";
    }
    if (estimate.voltageUv > 5500000 || quickChargeTypeFromPowerMw(estimate.powerMw) >= 2) {
        return std::to_string((estimate.powerMw + 500) / 1000);
    }
    return "0";
}

std::string readFastChargeModeStatus(std::string* source) {
    std::string originalSource;
    std::string originalValue;
    if (readFirstValue({"/sys/class/xm_power/fuelgauge/strategy_fg/fast_charge"}, &originalValue,
                       &originalSource)) {
        if (source != nullptr) {
            *source = originalSource;
        }
        return originalValue.empty() ? "0" : originalValue;
    }

    const UsbPowerEstimate estimate = readUsbPowerEstimate();
    if (source != nullptr) {
        *source = estimate.source;
    }
    if (!estimate.valid) {
        maybeEmitQuickChargeUevent(0, estimate, "getFastChargeModeStatus:offline");
        return "0";
    }
    return quickChargeTypeFromPowerMw(estimate.powerMw) > 0 ? "1" : "0";
}

std::string readChargingPowerMax(std::string* source) {
    std::string originalSource;
    std::string originalValue;
    if (readFirstValue({"/sys/class/xm_power/charger/charger_common/power_max"}, &originalValue,
                       &originalSource)) {
        if (source != nullptr) {
            *source = originalSource;
        }
        return originalValue.empty() ? "0" : originalValue;
    }

    const UsbPowerEstimate estimate = readUsbPowerEstimate();
    if (source != nullptr) {
        *source = estimate.source;
    }
    if (!estimate.valid) {
        maybeEmitQuickChargeUevent(0, estimate, "getChargingPowerMax:offline");
        return "0";
    }
    maybeEmitQuickChargeUevent(quickChargeTypeFromPowerMw(estimate.powerMw), estimate,
                               "getChargingPowerMax");
    return std::to_string(advertisedChargingPowerW(estimate));
}

std::string readBatteryChargeType(std::string* source) {
    std::string value;
    std::string valueSource;
    const UsbPowerEstimate estimate = readUsbPowerEstimate();
    if (readFirstValue({"/sys/class/xm_power/charger/charger_common/real_type",
                        "/sys/class/power_supply/usb/real_type",
                        "/sys/class/power_supply/usb/type"},
                       &value, &valueSource)) {
        if (source != nullptr) {
            *source = valueSource;
        }
        const std::string normalized = normalizeChargeType(value, estimate);
        maybeEmitQuickChargeUevent(estimate.valid ? quickChargeTypeFromPowerMw(estimate.powerMw)
                                                  : 0,
                                   estimate, "getBatteryChargeType:raw");
        return normalized.empty() ? "USB" : normalized;
    }

    if (readIntProperty("persist.vendor.micharge.battery_type_numeric", 0) != 0) {
        std::string quickSource;
        const std::string quickValue = readQuickChargeType(&quickSource);
        if (source != nullptr) {
            *source = "quick_enum " + quickSource;
        }
        return quickValue;
    }

    if (source != nullptr) {
        *source = estimate.source;
    }
    maybeEmitQuickChargeUevent(estimate.valid ? quickChargeTypeFromPowerMw(estimate.powerMw) : 0,
                               estimate, "getBatteryChargeType:synth");
    return synthesizeChargeType(estimate);
}

std::string resolveNode(const std::string& rawKey) {
    std::string key = trim(rawKey);
    if (key.empty()) {
        return {};
    }
    if (key.rfind("/sys/", 0) == 0) {
        return key;
    }
    if (key.rfind("sys/", 0) == 0) {
        return "/" + key;
    }

    for (const NodeAlias& alias : kNodeAliases) {
        if (key == alias.key) {
            return alias.path;
        }
    }

    const std::string xmPrefix = "xm_power/";
    if (key.rfind(xmPrefix, 0) == 0) {
        return "/sys/class/" + key;
    }

    const std::string powerSupplyPrefix = "power_supply/";
    if (key.rfind(powerSupplyPrefix, 0) == 0) {
        return "/sys/class/" + key;
    }

    return {};
}

const char* methodName(transaction_code_t code) {
    static constexpr const char* kNames[] = {
            "",
            "getBatteryAuthentic",
            "getBatteryCapacity",
            "getBatteryChargeFull",
            "getBatteryChargeType",
            "getBatteryCycleCount",
            "getBatteryIbat",
            "getBatteryResistance",
            "getBatterySoh",
            "getBatteryTbat",
            "getBatteryThermaLevel",
            "getBatteryVbat",
            "getBtTransferStartState",
            "getCarChargingType",
            "getChargingPowerMax",
            "getCoolModeState",
            "getFastChargeModeStatus",
            "getInputSuspendState",
            "getMiChargePath",
            "getNightChargingState",
            "getPSValue",
            "getPdApdoMax",
            "getPdAuthentication",
            "getQuickChargeType",
            "getSBState",
            "getSocDecimal",
            "getSocDecimalRate",
            "getTxAdapt",
            "getUsbCurrent",
            "getUsbVoltage",
            "getWirelessChargingStatus",
            "getWirelessFwStatus",
            "getWirelessReverseStatus",
            "isBatteryLifeFunctionSupported",
            "isDPConnected",
            "isFunctionSupported",
            "isUSB32",
            "isWirelessChargingSupported",
            "isWiressFwUpdateSupported",
            "setBtState",
            "setBtTransferStartState",
            "setCoolModeState",
            "setInputSuspendState",
            "setMiChargePath",
            "setNightChargingState",
            "setRxCr",
            "setSBState",
            "setSmCountReset",
            "setUpdateWirelessFw",
            "setWirelessChargingEnabled",
            "setWlsTxSpeed",
            "getTypeCCommonInfo",
            "setTypeCCommonInfo",
            "getChargeCommonInfo",
            "setChargeCommonInfo",
            "getBatteryCommonInfo",
            "setBatteryCommonInfo",
    };

    if (code < sizeof(kNames) / sizeof(kNames[0])) {
        return kNames[code];
    }
    if (code == kGetInterfaceHash) {
        return "getInterfaceHash";
    }
    if (code == kGetInterfaceVersion) {
        return "getInterfaceVersion";
    }
    return "unknown";
}

std::string readSimpleGetter(transaction_code_t code, std::string* source) {
    switch (code) {
        case 1:
            return readBatteryAuthentic(source);
        case 2:
            return readFirst({"/sys/class/power_supply/battery/capacity"}, "0", source);
        case 3:
            return readFirst({"/sys/class/power_supply/battery/charge_full"}, "0", source);
        case 4:
            return readBatteryChargeType(source);
        case 5:
            return readFirst({"/sys/class/power_supply/battery/cycle_count",
                              "/sys/class/xm_power/fg_master/cyclecount"},
                             "0", source);
        case 6:
            return readFirst({"/sys/class/power_supply/battery/current_now"}, "0", source);
        case 7:
            return readFirst({"/sys/class/xm_power/battery/resistance_id"}, "0", source);
        case 8:
            return readFirst({"/sys/class/xm_power/fg_master/soh",
                              "/sys/class/xm_power/fg_master/ui_soh"},
                             "0", source);
        case 9:
            return readFirst({"/sys/class/power_supply/battery/temp"}, "0", source);
        case 10:
            return readFirst({"/sys/class/power_supply/battery/system_temp_level"}, "0", source);
        case 11:
            return readFirst({"/sys/class/power_supply/battery/voltage_now"}, "0", source);
        case 12:
            return readFirst({"/sys/class/xm_power/wireless_master/bt_transfer_start"}, "0", source);
        case 13:
            return readFirst({"/sys/class/xm_power/charger/wls_basic_charge/wls_car_adapter"}, "0",
                             source);
        case 14:
            return readChargingPowerMax(source);
        case 15:
            return readSmartChargeMode(source);
        case 16:
            return readFastChargeModeStatus(source);
        case 17:
            return readFirst({"/sys/class/xm_power/charger/charge_interface/input_suspend"}, "0",
                             source);
        case 19:
            return readFirst({"/sys/class/xm_power/charger/smart_charge/smart_night"}, "0", source);
        case 20:
            return readFirst({"/sys/class/power_supply/battery/status"}, "0", source);
        case 21:
            return readPdApdoMax(source);
        case 22:
            return readPdAuthentication(source);
        case 23:
            return readQuickChargeType(source);
        case 24:
            return readFirst({"/sys/class/xm_power/charger/smart_charge/smart_batt"}, "0", source);
        case 25:
            return readFirst({"/sys/class/xm_power/fuelgauge/strategy_fg/soc_decimal"}, "0", source);
        case 26:
            return readFirst({"/sys/class/xm_power/fuelgauge/strategy_fg/soc_decimal_rate"}, "0",
                             source);
        case 27:
            return readFirst({"/sys/class/xm_power/wireless_master/tx_adapter"}, "0", source);
        case 28:
            return readFirst({"/sys/class/power_supply/usb/current_now"}, "0", source);
        case 29:
            return readFirst({"/sys/class/power_supply/usb/voltage_now"}, "0", source);
        case 30:
            return readFirst({"/sys/class/xm_power/charger/wls_basic_charge/wls_quick_chg_status",
                              "/sys/class/power_supply/wireless/online"},
                             "0", source);
        case 31:
            return readFirst({"/sys/class/xm_power/charger/wls_rev_charge/wls_fw_state"}, "0",
                             source);
        case 32:
            return readFirst({"/sys/class/xm_power/charger/wls_rev_charge/reverse_chg_state",
                              "/sys/class/xm_power/charger/wls_rev_charge/reverse_chg_mode"},
                             "0", source);
        default:
            if (source != nullptr) {
                *source = "fallback";
            }
            return "0";
    }
}

std::string readGenericGetter(const std::string& key, std::string* source) {
    const std::string path = resolveNode(key);
    if (!path.empty()) {
        std::string value;
        if (readFile(path.c_str(), &value)) {
            if (source != nullptr) {
                *source = path;
            }
            return value.empty() ? "0" : value;
        }
    }

    const std::string normalizedKey = trim(key);
    if (normalizedKey == "authentic" || normalizedKey == "slave_authentic" ||
        normalizedKey.find("strategy_fg/authentic") != std::string::npos ||
        normalizedKey.find("strategy_fg/slave_authentic") != std::string::npos) {
        return readBatteryAuthentic(source);
    }
    if (normalizedKey == "verified" || normalizedKey == "pd_verified" ||
        normalizedKey == "pd_verifed" || normalizedKey == "pd_auth" ||
        normalizedKey.find("strategy_pd_auth/verified") != std::string::npos ||
        normalizedKey.find("qcom-battery/pd_verifed") != std::string::npos ||
        normalizedKey.find("qcom-battery/pd_verified") != std::string::npos) {
        return readPdAuthentication(source);
    }
    if (normalizedKey == "verify_process" ||
        normalizedKey.find("strategy_pd_auth/verify_process") != std::string::npos ||
        normalizedKey.find("qcom-battery/verify_process") != std::string::npos) {
        return readPdVerifyProcess(source);
    }
    if (normalizedKey == "is_pd_adapter" ||
        normalizedKey.find("strategy_pd_auth/is_pd_adapter") != std::string::npos) {
        return readIsPdAdapter(source);
    }
    if (normalizedKey == "apdo_max" ||
        normalizedKey.find("/sys/class/xm_power/typec/apdo_max") != std::string::npos) {
        return readPdApdoMax(source);
    }
    if (normalizedKey == "quick_charge_type") {
        return readQuickChargeType(source);
    }
    if (normalizedKey == "fast_charge") {
        return readFastChargeModeStatus(source);
    }
    if (normalizedKey == "smart_chg") {
        return readSmartChargeMode(source);
    }
    if (normalizedKey == "power_max") {
        return readChargingPowerMax(source);
    }
    if (normalizedKey == "real_type") {
        return readBatteryChargeType(source);
    }
    if (normalizedKey == "typec_portx_dpdm_attach") {
        std::string onlineSource;
        const std::string value = readUsbOnlineValue(&onlineSource);
        if (source != nullptr) {
            *source = onlineSource;
        }
        if (value == "0") {
            maybeEmitQuickChargeUevent(0, readUsbPowerEstimate(),
                                       "getMiChargePath:typec_portx_dpdm_attach:offline");
        } else if (value == "1") {
            markQuickChargeAttachOnline("getMiChargePath:typec_portx_dpdm_attach:online");
        }
        return value;
    }
    if (normalizedKey == "getTypecPortNum") {
        if (source != nullptr) {
            *source = "property persist.vendor.micharge.typec_port_num";
        }
        return std::to_string(readIntProperty("persist.vendor.micharge.typec_port_num", 1));
    }
    if (normalizedKey == "support_batt_manager_4p0") {
        if (source != nullptr) {
            *source = "static unsupported";
        }
        return "0";
    }

    if (source != nullptr) {
        *source = path.empty() ? "unresolved" : path;
    }
    return "0";
}

binder_status_t writeStatusHeader(AParcel* out) {
    AStatus* ok = AStatus_newOk();
    if (ok == nullptr) {
        return STATUS_NO_MEMORY;
    }

    const binder_status_t status = AParcel_writeStatusHeader(out, ok);
    AStatus_delete(ok);
    return status;
}

binder_status_t writeStringReply(AParcel* out, const std::string& value) {
    binder_status_t status = writeStatusHeader(out);
    if (status != STATUS_OK) {
        return status;
    }
    return AParcel_writeString(out, value.c_str(), static_cast<int32_t>(value.size()));
}

binder_status_t writeIntReply(AParcel* out, int32_t value) {
    binder_status_t status = writeStatusHeader(out);
    if (status != STATUS_OK) {
        return status;
    }
    return AParcel_writeInt32(out, value);
}

binder_status_t writeBoolReply(AParcel* out, bool value) {
    binder_status_t status = writeStatusHeader(out);
    if (status != STATUS_OK) {
        return status;
    }
    return AParcel_writeBool(out, value);
}

void logStringCall(const char* name, const std::string& args, const std::string& value,
                   const std::string& source) {
    LOGI("%s(%s) -> \"%s\" source=%s caller=%d:%d", name, args.c_str(), value.c_str(),
         source.c_str(), AIBinder_getCallingUid(), AIBinder_getCallingPid());
}

void logIntCall(const char* name, const std::string& args, int32_t value) {
    LOGI("%s(%s) -> %d caller=%d:%d", name, args.c_str(), value, AIBinder_getCallingUid(),
         AIBinder_getCallingPid());
}

void logBoolCall(const char* name, const std::string& args, bool value) {
    LOGI("%s(%s) -> %s caller=%d:%d", name, args.c_str(), value ? "true" : "false",
         AIBinder_getCallingUid(), AIBinder_getCallingPid());
}

binder_status_t readStringArg(const AParcel* in, std::string* value) {
    value->clear();
    return ndk::AParcel_readString(in, value);
}

bool isFunctionSupportedByName(const std::string& name) {
    if (name == "support_batt_manager_4p0") {
        return false;
    }
    if (name == "wireless_charging" || name == "wireless_fw_update") {
        return readIntProperty("persist.vendor.micharge.wireless_supported", 0) != 0;
    }
    return true;
}

bool boolGetterValue(transaction_code_t code) {
    switch (code) {
        case 33:
            return true;
        case 34:
            return readFirst({"/sys/class/xm_power/typec/has_dp"}, "0", nullptr) == "1";
        case 36:
            return pathExists("/sys/class/xm_power/typec/super_speed") &&
                   readFirst({"/sys/class/xm_power/typec/super_speed"}, "0", nullptr) == "1";
        case 37:
            if (readIntProperty("persist.vendor.micharge.wireless_supported", 0) != 0) {
                return true;
            }
            return readFirst({"/sys/class/power_supply/wireless/online"}, "0", nullptr) == "1";
        case 38:
            return pathExists("/sys/class/xm_power/charger/wls_rev_charge/wireless_chip_fw");
        default:
            return false;
    }
}

binder_status_t onTransact(AIBinder*, transaction_code_t code, const AParcel* in, AParcel* out) {
    const char* name = methodName(code);

    if (code == kGetInterfaceVersion) {
        logIntCall(name, "", kInterfaceVersion);
        return writeIntReply(out, kInterfaceVersion);
    }

    if (code == kGetInterfaceHash) {
        logStringCall(name, "", kInterfaceHash, "static");
        return writeStringReply(out, kInterfaceHash);
    }

    if (code >= 1 && code <= 32 && code != 18) {
        std::string source;
        const std::string value = readSimpleGetter(code, &source);
        logStringCall(name, "", value, source);
        return writeStringReply(out, value);
    }

    if (code == 18 || code == 51 || code == 53 || code == 55) {
        std::string key;
        binder_status_t status = readStringArg(in, &key);
        if (status != STATUS_OK) {
            LOGW("%s: failed to read string arg status=%d", name, status);
            return status;
        }

        std::string source;
        const std::string value = readGenericGetter(key, &source);
        logStringCall(name, "key=\"" + key + "\"", value, source);
        return writeStringReply(out, value);
    }

    if (code == 35) {
        std::string functionName;
        binder_status_t status = readStringArg(in, &functionName);
        if (status != STATUS_OK) {
            LOGW("%s: failed to read string arg status=%d", name, status);
            return status;
        }

        const bool supported = isFunctionSupportedByName(functionName);
        logBoolCall(name, "name=\"" + functionName + "\"", supported);
        return writeBoolReply(out, supported);
    }

    if (code >= 33 && code <= 38) {
        const bool value = boolGetterValue(code);
        logBoolCall(name, "", value);
        return writeBoolReply(out, value);
    }

    if (code == 52 || code == 54 || code == 56 || code == 43) {
        std::string key;
        std::string value;
        binder_status_t status = readStringArg(in, &key);
        if (status != STATUS_OK) {
            LOGW("%s: failed to read key arg status=%d", name, status);
            return status;
        }
        status = readStringArg(in, &value);
        if (status != STATUS_OK) {
            LOGW("%s: failed to read value arg status=%d", name, status);
            return status;
        }

        if (code == 43 && trim(key) == "smart_chg") {
            handleSmartChargeValue(value, "setMiChargePath:smart_chg");
        }
        logIntCall(name, "key=\"" + key + "\" value=\"" + value + "\"", 0);
        return writeIntReply(out, 0);
    }

    if ((code >= 39 && code <= 48) || code == 50) {
        std::string value;
        binder_status_t status = readStringArg(in, &value);
        if (status != STATUS_OK) {
            LOGW("%s: failed to read string arg status=%d", name, status);
            return status;
        }

        logIntCall(name, "value=\"" + value + "\"", 0);
        return writeIntReply(out, 0);
    }

    if (code == 49) {
        bool enabled = false;
        binder_status_t status = AParcel_readBool(in, &enabled);
        if (status != STATUS_OK) {
            LOGW("%s: failed to read bool arg status=%d", name, status);
            return status;
        }

        logIntCall(name, std::string("enabled=") + (enabled ? "true" : "false"), 0);
        return writeIntReply(out, 0);
    }

    LOGW("unknown transaction code=%u caller=%d:%d", code, AIBinder_getCallingUid(),
         AIBinder_getCallingPid());
    return STATUS_UNKNOWN_TRANSACTION;
}

void* onCreate(void* args) {
    return args;
}

void onDestroy(void*) {}

void* binderSymbol(const char* symbol) {
    static void* handle = dlopen("libbinder_ndk.so", RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
        LOGE("dlopen(libbinder_ndk.so) failed: %s", dlerror());
        return nullptr;
    }
    return dlsym(handle, symbol);
}

}  // namespace

int main() {
    LOGI("starting %s", kInstance);
    initializeSmartChargeState();

    AIBinder_Class* clazz = AIBinder_Class_define(kDescriptor, onCreate, onDestroy, onTransact);
    if (clazz == nullptr) {
        LOGE("AIBinder_Class_define failed");
        return 1;
    }

    AIBinder* binder = AIBinder_new(clazz, nullptr);
    if (binder == nullptr) {
        LOGE("AIBinder_new failed");
        return 1;
    }

    auto markVintf = reinterpret_cast<MarkVintfStabilityFn>(
            binderSymbol("AIBinder_markVintfStability"));
    if (markVintf != nullptr) {
        markVintf(binder);
    } else {
        LOGW("AIBinder_markVintfStability not found");
    }

    auto addService =
            reinterpret_cast<AddServiceFn>(binderSymbol("AServiceManager_addService"));
    auto setThreadPool = reinterpret_cast<SetThreadPoolFn>(
            binderSymbol("ABinderProcess_setThreadPoolMaxThreadCount"));
    auto joinThreadPool =
            reinterpret_cast<JoinThreadPoolFn>(binderSymbol("ABinderProcess_joinThreadPool"));

    if (addService == nullptr || setThreadPool == nullptr || joinThreadPool == nullptr) {
        LOGE("required binder service-manager symbols missing add=%p setPool=%p join=%p",
             reinterpret_cast<void*>(addService), reinterpret_cast<void*>(setThreadPool),
             reinterpret_cast<void*>(joinThreadPool));
        AIBinder_decStrong(binder);
        return 1;
    }

    setThreadPool(0);
    const binder_status_t status = addService(binder, kInstance);
    if (status != STATUS_OK) {
        LOGE("AServiceManager_addService(%s) failed status=%d", kInstance, status);
        AIBinder_decStrong(binder);
        return 1;
    }

    LOGI("registered %s", kInstance);
    joinThreadPool();

    AIBinder_decStrong(binder);
    return 0;
}
