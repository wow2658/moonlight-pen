// Local utility for this Samsung 940X3M: control only the verified finger-touch HID collection.
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cfgmgr32.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <stdexcept>
#include "device-profile.local.h"

namespace {
constexpr wchar_t touchId[] = MOONLIGHT_TOUCH_ID;
constexpr wchar_t touchParent[] = MOONLIGHT_TOUCH_PARENT;
constexpr wchar_t penId[] = MOONLIGHT_PEN_ID;
constexpr wchar_t penParent[] = MOONLIGHT_PEN_PARENT;

void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
DEVINST locate(const wchar_t* id) {
    DEVINST node = 0;
    require(CM_Locate_DevNodeW(&node, const_cast<wchar_t*>(id), CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS,
            "The verified device was not found. No other device will be selected.");
    return node;
}
std::wstring parent_id(DEVINST node) {
    DEVINST parent = 0; wchar_t id[MAX_DEVICE_ID_LEN] = {};
    require(CM_Get_Parent(&parent, node, 0) == CR_SUCCESS &&
            CM_Get_Device_IDW(parent, id, MAX_DEVICE_ID_LEN, 0) == CR_SUCCESS, "Cannot verify device parent.");
    return id;
}
bool usage_is(DEVINST node, const wchar_t* expected) {
    wchar_t buffer[4096] = {}; ULONG size = sizeof(buffer), type = 0;
    require(CM_Get_DevNode_Registry_PropertyW(node, CM_DRP_HARDWAREID, &type, buffer, &size, 0) == CR_SUCCESS &&
            type == REG_MULTI_SZ && size <= sizeof(buffer) && size >= 2 * sizeof(wchar_t), "Cannot verify HID usage.");
    // Registry data must be double-NUL terminated before walking the multi-string.
    const auto count = size / sizeof(wchar_t);
    require(buffer[count - 1] == 0 && buffer[count - 2] == 0, "Malformed HID hardware identifiers.");
    for (const wchar_t* id = buffer; *id; id += wcslen(id) + 1)
        if (_wcsicmp(id, expected) == 0) return true;
    return false;
}
ULONG problem(DEVINST node) {
    ULONG status = 0, code = 0;
    require(CM_Get_DevNode_Status(&status, &code, node, 0) == CR_SUCCESS, "Cannot query device status.");
    require((status & DN_HAS_PROBLEM) || code == 0, "Inconsistent device status.");
    return code;
}
void verify_pen() {
    const auto node = locate(penId);
    require(_wcsicmp(parent_id(node).c_str(), penParent) == 0 &&
            usage_is(node, L"HID_DEVICE_UP:000D_U:0002"), "The integrated pen identity changed.");
    require(problem(node) == 0 && problem(locate(penParent)) == 0, "The S Pen device is not healthy.");
}
DEVINST verified_touch() {
    const auto node = locate(touchId);
    require(_wcsicmp(parent_id(node).c_str(), touchParent) == 0 &&
            usage_is(node, L"HID_DEVICE_UP:000D_U:0004"), "The finger-touch identity changed.");
    const auto code = problem(node);
    require(code == 0 || code == CM_PROB_DISABLED, "The touch device has an unrelated problem.");
    return node;
}
bool enabled() { return problem(verified_touch()) == 0; }
void change_touch(bool enable) {
    auto node = verified_touch();
    if (enable && problem(node) == 0) return;
    if (!enable) verify_pen();
    auto result = enable ? CM_Enable_DevNode(node, 0) : CM_Disable_DevNode(node, CM_DISABLE_UI_NOT_OK);
    require(result == CR_SUCCESS, "Windows could not change the touch device. Administrator rights are required.");
    bool confirmed = false;
    for (unsigned i = 0; i < 20; ++i) {
        Sleep(100);
        if (enabled() == enable) { confirmed = true; break; }
    }
    if (!confirmed) {
        if (!enable) CM_Enable_DevNode(node, 0);
        throw std::runtime_error("The requested touch state could not be confirmed; enabling touch was attempted.");
    }
    if (!enable) {
        try { verify_pen(); }
        catch (...) { CM_Enable_DevNode(node, 0); throw; }
    }
}
void output(const std::string& text) {
    DWORD written = 0;
    HANDLE stream = GetStdHandle(STD_OUTPUT_HANDLE);
    if (stream && stream != INVALID_HANDLE_VALUE) WriteFile(stream, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
}
LRESULT CALLBACK banner_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    if (message == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(window, &ps); RECT r; GetClientRect(window, &r);
        HBRUSH brush = CreateSolidBrush(RGB(35,38,44)); FillRect(dc, &r, brush); DeleteObject(brush);
        SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(240,242,245));
        HFONT font = CreateFontW(-20,0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Malgun Gothic");
        auto old = SelectObject(dc, font); wchar_t title[128]; GetWindowTextW(window, title, 128);
        DrawTextW(dc, title, -1, &r, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        SelectObject(dc, old); DeleteObject(font); EndPaint(window, &ps); return 0;
    }
    if (message == WM_TIMER) { DestroyWindow(window); return 0; }
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(window, message, wp, lp);
}
void show_state(bool on) {
    WNDCLASSW wc = {}; wc.lpfnWndProc = banner_proc; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"LaptopFingerTouchStatus";
    RegisterClassW(&wc);
    POINT cursor; GetCursorPos(&cursor); MONITORINFO info = {}; info.cbSize = sizeof(info);
    GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY), &info);
    const int width = 420, height = 58;
    const wchar_t* title = on ? L"손가락 터치 켜짐" : L"손가락 터치 꺼짐 · S펜 유지";
    HWND w = CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE, wc.lpszClassName, title, WS_POPUP,
        info.rcWork.left + (info.rcWork.right-info.rcWork.left-width)/2, info.rcWork.top+36, width, height, nullptr,nullptr,wc.hInstance,nullptr);
    if (!w) return;
    ShowWindow(w, SW_SHOWNOACTIVATE); UpdateWindow(w); SetTimer(w, 1, 1100, nullptr);
    MSG msg; while (GetMessageW(&msg,nullptr,0,0)>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
}
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    bool quiet = false;
    try {
        int argc = 0; auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        require(argv != nullptr, "Cannot parse arguments.");
        const std::wstring command = argc >= 2 ? argv[1] : L"--toggle";
        require(argc <= 3, "Too many arguments.");
        quiet = command == L"--status" || command == L"--self-test" || command == L"--probe-hotkeys" || (argc == 3 && wcscmp(argv[2], L"--quiet") == 0);
        LocalFree(argv);
        if (command == L"--probe-hotkeys") {
            for (unsigned key : {VK_F10, VK_F11}) {
                const bool available = RegisterHotKey(nullptr, key, MOD_CONTROL|MOD_ALT|MOD_NOREPEAT, key) != FALSE;
                const auto error = GetLastError();
                if (available) UnregisterHotKey(nullptr, key);
                require(available || error == ERROR_HOTKEY_ALREADY_REGISTERED, "Cannot inspect global shortcut registration.");
                output(std::string(key == VK_F10 ? "Ctrl+Alt+F10=" : "Ctrl+Alt+F11=") + (available ? "available\n" : "registered\n"));
            }
            return 0;
        }
        HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\Samsung940X3MFingerTouchToggle");
        require(mutex != nullptr, "Cannot create operation lock.");
        const DWORD lock = WaitForSingleObject(mutex, 0);
        if (lock != WAIT_OBJECT_0 && lock != WAIT_ABANDONED) { CloseHandle(mutex); return 4; }
        struct Guard { HANDLE h; ~Guard(){ReleaseMutex(h);CloseHandle(h);} } guard{mutex};
        if (command == L"--status") {
            verify_pen(); output(enabled() ? "touch=enabled; pen=healthy\n" : "touch=disabled; pen=healthy\n"); return 0;
        }
        if (command == L"--self-test") {
            verify_pen(); const bool before = enabled();
            try {
                change_touch(false); require(!enabled(), "Disable verification failed."); verify_pen();
                output("PASS: finger touch disabled; separate S Pen device healthy\n");
                change_touch(true); require(enabled(), "Enable verification failed."); verify_pen();
                output("PASS: finger touch enabled; separate S Pen device healthy\n");
                change_touch(before);
            } catch (...) { try { change_touch(true); } catch (...) {} throw; }
            output("PASS: initial touch state restored\n"); return 0;
        }
        require(command == L"--toggle" || command == L"--on" || command == L"--off", "Unknown operation.");
        const bool target = command == L"--on" || (command == L"--toggle" && !enabled());
        change_touch(target); output(target ? "touch=enabled\n" : "touch=disabled\n");
        if (!quiet) show_state(target);
        return 0;
    } catch (const std::exception& error) {
        output(std::string("ERROR: ") + error.what() + "\n");
        if (!quiet) {
            const std::string message = error.what();
            std::wstring wide(message.begin(), message.end());
            MessageBoxW(nullptr, (L"손가락 터치를 변경하지 못했습니다.\n\n"+wide).c_str(), L"손가락 터치 전환", MB_OK|MB_ICONERROR);
        }
        return 1;
    }
}
