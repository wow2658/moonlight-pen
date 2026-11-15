#include "../touch-toggle/device-selection.h"
#include <iostream>
#include <cassert>
using namespace VerifiedTouch;
static void reject(const std::wstring& model, const std::vector<Candidate>& items) {
    bool rejected = false;
    try { (void)select(model, items); } catch (const std::runtime_error&) { rejected = true; }
    assert(rejected);
}
int main() {
    const Candidate touch{L"HID\\STMD1234&COL01\\EXAMPLE-A", L"ACPI\\STMD1234\\EXAMPLE-A", true, false};
    const Candidate pen{L"HID\\WCOM0032&COL02\\EXAMPLE-B", L"ACPI\\WCOM0032\\EXAMPLE-B", false, true};
    assert(select(L"940X3M", {touch,pen}).touch == touch.id);
    auto changed = touch; changed.id = L"HID\\STMD1234&COL01\\AFTER-FORMAT";
    assert(select(L"940X3M", {changed,pen}).touch == changed.id);
    reject(L"OTHER-MODEL", {touch,pen});
    reject(L"940X3M", {}); reject(L"940X3M", {touch}); reject(L"940X3M", {pen});
    reject(L"940X3M", {touch,touch,pen}); reject(L"940X3M", {touch,pen,pen});
    auto bad = touch; bad.parent = pen.parent; reject(L"940X3M", {bad,pen});
    bad = touch; bad.finger = false; reject(L"940X3M", {bad,pen});
    bad = touch; bad.pen = true; reject(L"940X3M", {bad,pen});
    bad = pen; bad.finger = true; reject(L"940X3M", {touch,bad});
    bad = pen; bad.pen = false; reject(L"940X3M", {touch,bad});
    bad = touch; bad.id = L"HID\\OTHER\\EXAMPLE"; reject(L"940X3M", {bad,pen});
    bad = touch; bad.id = L"HID\\STMD1234&COL010\\EXAMPLE"; reject(L"940X3M", {bad,pen});
    std::cout << "PASS: supported model, post-format suffix, ambiguity, HID usage, parent and prefix rejection\n";
}
