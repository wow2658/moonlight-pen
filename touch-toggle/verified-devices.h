#pragma once
#include "device-selection.h"
#include <algorithm>
#include <cwctype>

namespace VerifiedTouch {
inline std::wstring uppercase(std::wstring text) {
    std::transform(text.begin(), text.end(), text.begin(), [](wchar_t c) { return static_cast<wchar_t>(towupper(c)); });
    return text;
}
inline bool has_usage(DEVINST node, const wchar_t* expected) {
    wchar_t values[4096] = {}; ULONG size = sizeof(values), type = 0;
    if (CM_Get_DevNode_Registry_PropertyW(node, CM_DRP_HARDWAREID, &type, values, &size, 0) != CR_SUCCESS ||
        type != REG_MULTI_SZ || size < 2 * sizeof(wchar_t) || size > sizeof(values) || size % sizeof(wchar_t))
        throw std::runtime_error("Cannot verify digitizer HID usage.");
    const auto count = size / sizeof(wchar_t);
    if (values[count - 1] || values[count - 2]) throw std::runtime_error("Invalid HID property termination.");
    for (const wchar_t* value = values; *value; value += wcslen(value) + 1)
        if (_wcsicmp(value, expected) == 0) return true;
    return false;
}
inline std::wstring model_name() {
    wchar_t value[256] = {}; DWORD size = sizeof(value);
    const auto result = RegGetValueW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemProductName",
        RRF_RT_REG_SZ, nullptr, value, &size);
    if (result != ERROR_SUCCESS || size < sizeof(wchar_t) || size > sizeof(value) || value[size / sizeof(wchar_t) - 1])
        throw std::runtime_error("Cannot verify laptop model.");
    return uppercase(value);
}
inline Profile discover() {
    const auto model = model_name();
    if (model != L"940X3M") return select(model, {});
    std::vector<wchar_t> list;
    bool complete = false;
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        ULONG count = 0;
        if (CM_Get_Device_ID_List_SizeW(&count, nullptr, CM_GETIDLIST_FILTER_PRESENT) != CR_SUCCESS || count < 2 || count > 1024 * 1024)
            throw std::runtime_error("Cannot size present-device inventory.");
        list.assign(count, 0);
        const auto result = CM_Get_Device_ID_ListW(nullptr, list.data(), count, CM_GETIDLIST_FILTER_PRESENT);
        if (result == CR_BUFFER_SMALL) continue;
        if (result != CR_SUCCESS || list[count - 1] || list[count - 2])
            throw std::runtime_error("Cannot read a complete present-device inventory.");
        complete = true; break;
    }
    if (!complete) throw std::runtime_error("Device inventory changed; retry after Windows finishes device setup.");
    std::vector<Candidate> candidates;
    for (const wchar_t* value = list.data(); *value; value += wcslen(value) + 1) {
        const auto id = uppercase(value);
        if (!starts(id, L"HID\\STMD1234&COL01\\") && !starts(id, L"HID\\WCOM0032&COL02\\")) continue;
        DEVINST node = 0, parent = 0; wchar_t parentId[MAX_DEVICE_ID_LEN] = {};
        if (CM_Locate_DevNodeW(&node, const_cast<wchar_t*>(value), CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS ||
            CM_Get_Parent(&parent, node, 0) != CR_SUCCESS || CM_Get_Device_IDW(parent, parentId, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS)
            throw std::runtime_error("Cannot verify the digitizer parent.");
        candidates.push_back({id, uppercase(parentId), has_usage(node, L"HID_DEVICE_UP:000D_U:0004"), has_usage(node, L"HID_DEVICE_UP:000D_U:0002")});
    }
    return select(model, candidates);
}
}
