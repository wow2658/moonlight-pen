#pragma once
#include <string>
#include <vector>
#include <stdexcept>

namespace VerifiedTouch {
struct Candidate {
    std::wstring id;
    std::wstring parent;
    bool finger = false;
    bool pen = false;
};
struct Profile { std::wstring touch, touchParent, pen, penParent; };
inline bool starts(const std::wstring& value, const wchar_t* prefix) {
    return value.rfind(prefix, 0) == 0;
}
// Inputs are normalized to uppercase by the native enumerator. Device instance
// suffixes remain local; no user/account/device-specific value is compiled in.
inline Profile select(const std::wstring& model, const std::vector<Candidate>& devices) {
    if (model != L"940X3M") throw std::runtime_error("Unsupported model. This release supports Samsung 940X3M only.");
    const Candidate* touch = nullptr;
    const Candidate* pen = nullptr;
    for (const auto& device : devices) {
        if (starts(device.id, L"HID\\STMD1234&COL01\\")) {
            if (touch || !device.finger || device.pen || !starts(device.parent, L"ACPI\\STMD1234\\"))
                throw std::runtime_error("Finger-touch identity is ambiguous or invalid. No device was changed.");
            touch = &device;
        }
        if (starts(device.id, L"HID\\WCOM0032&COL02\\")) {
            if (pen || !device.pen || device.finger || !starts(device.parent, L"ACPI\\WCOM0032\\"))
                throw std::runtime_error("Separate S Pen identity is ambiguous or invalid. No device was changed.");
            pen = &device;
        }
    }
    if (!touch || !pen || touch->id == pen->id || touch->parent == pen->parent)
        throw std::runtime_error("Expected separate finger-touch and S Pen devices were not found. No device was changed.");
    return {touch->id, touch->parent, pen->id, pen->parent};
}
}
