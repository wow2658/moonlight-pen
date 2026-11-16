// SPDX-License-Identifier: GPL-3.0-or-later
// Runs in its OWN window station. Never reads/writes the user's interactive clipboard.
#include "../shared/clipboard/windows.h"
#include <iostream>
#include <cstdlib>
#include <fstream>
using namespace pen_clipboard;
#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL line " << __LINE__ << ": " #x " error=" << GetLastError() << "\n"; std::exit(1); } } while (0)

static HWND owner;
static void set_text(const std::string& text) {
    const auto deadline = GetTickCount64() + 1000;
    for (;;) {
        DWORD sequence;
        auto result = write_clipboard(owner, text, GetClipboardSequenceNumber(), sequence);
        if (result == WriteResult::written || result == WriteResult::unchanged) return;
        CHECK(result == WriteResult::busy || result == WriteResult::changed);
        CHECK(GetTickCount64() < deadline);
        Sleep(1);
    }
}
static void touch_metadata() {
    const auto deadline = GetTickCount64() + 1000;
    while (!OpenClipboard(owner)) { CHECK(GetTickCount64() < deadline); Sleep(1); }
    const UINT format = RegisterClipboardFormatW(L"MoonlightPenTestMetadata");
    auto memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, 4);
    CHECK(memory && SetClipboardData(format, memory));
    CloseClipboard();
}
static void pump_messages() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
}

static int run_tests() {
    auto original_station = GetProcessWindowStation();
    auto original_desktop = GetThreadDesktop(GetCurrentThreadId());
    auto station_name = L"MoonlightClipboardTest-" + std::to_wstring(GetCurrentProcessId());
    HWINSTA station = CreateWindowStationW(station_name.c_str(), 0,
        WINSTA_ACCESSCLIPBOARD | WINSTA_CREATEDESKTOP | WINSTA_READATTRIBUTES | WINSTA_ENUMDESKTOPS, nullptr);
    if (!station && GetLastError() == ERROR_ACCESS_DENIED) {
        std::cout << "SKIP: isolated Windows station creation denied by this execution context; interactive clipboard was not accessed\n";
        return 77;
    }
    CHECK(station != nullptr && station != original_station);
    CHECK(SetProcessWindowStation(station));
    HDESK desktop = CreateDesktopW(L"Isolated", nullptr, nullptr, 0, GENERIC_ALL, nullptr);
    CHECK(desktop && SetThreadDesktop(desktop));
    WNDCLASSW wc{}; wc.lpfnWndProc = DefWindowProcW; wc.hInstance = GetModuleHandleW(nullptr); wc.lpszClassName = L"ClipboardTestOwner";
    RegisterClassW(&wc);
    owner = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
    CHECK(owner);
    const std::string sample = u8"Windows 실제 클립보드\r\n한글 English 😀🖊️\t끝";
    set_text(sample);
    auto snapshot = read_clipboard(owner);
    CHECK(snapshot && snapshot->text && *snapshot->text == sample);
    DWORD seq;
    const auto same_sequence = GetClipboardSequenceNumber();
    CHECK(write_clipboard(owner, sample, same_sequence, seq) == WriteResult::unchanged);
    CHECK(seq == same_sequence && GetClipboardSequenceNumber() == same_sequence);
    CHECK(write_clipboard(owner, "stale", snapshot->sequence - 1, seq) == WriteResult::changed);
    CHECK(*read_clipboard(owner)->text == sample);
    CHECK(write_clipboard(owner, "\xed\xa0\x80", snapshot->sequence, seq) == WriteResult::invalid);
    CHECK(*read_clipboard(owner)->text == sample);
    CHECK(OpenClipboard(owner));
    std::atomic<bool> saw_busy{false};
    std::thread contender([&] { saw_busy = !read_clipboard(nullptr); });
    contender.join(); CloseClipboard(); CHECK(saw_busy);
    set_text(""); CHECK(read_clipboard(owner)->text == std::optional<std::string>(""));
    std::string large(max_text, 'a'); set_text(large); CHECK(read_clipboard(owner)->text == large);
    CHECK(write_clipboard(owner, std::string(max_text + 1, 'x'), GetClipboardSequenceNumber(), seq) == WriteResult::invalid);
    CHECK(OpenClipboard(owner)); CHECK(EmptyClipboard()); CloseClipboard();
    CHECK(!read_clipboard(owner)->text);

    // Real worker + Windows clipboard against a host protocol endpoint.
    std::mutex wire_mutex;
    std::deque<std::string> to_host;
    std::string host_text = "host before sharing";
    unsigned host_writes = 0;
    WindowsBridge bridge(Role::client, [&](const auto& frame) {
        std::lock_guard<std::mutex> lock(wire_mutex); to_host.push_back(frame); return true;
    }, {}, [] { return true; }); // Our isolated non-interactive station has no input desktop.
    Endpoint host(Role::host, [&](const auto& frame) { return bridge.receive(frame); },
        [&](auto, auto text) { if (text) { host_text = *text; ++host_writes; } }, [](auto) {});
    auto poll = [&](auto predicate, unsigned timeout = 4000) {
        auto start = GetTickCount64();
        do {
            pump_messages();
            std::deque<std::string> frames;
            { std::lock_guard<std::mutex> lock(wire_mutex); frames.swap(to_host); }
            host.tick(GetTickCount64());
            for (const auto& frame : frames) host.receive(frame);
            if (predicate()) return true;
            Sleep(5);
        } while (GetTickCount64() - start < timeout);
        return false;
    };
    set_text("client before sharing");
    host.begin(GetTickCount64()); bridge.start();
    CHECK(poll([&] { return bridge.state() == State::connected; }));
    // The worker may still hold the clipboard lock after publishing connected.
    // Retry a busy read instead of dereferencing an empty optional.
    CHECK(poll([&] { auto s = read_clipboard(owner); return s && s->text == "client before sharing"; }));
    CHECK(host_writes == 0);
    set_text(sample); CHECK(poll([&] { return host_text == sample; }));
    CHECK(host_writes == 1);
    // History/format providers can change the sequence without a new text copy.
    for (int i = 0; i < 15; ++i) {
        touch_metadata();
        CHECK(!poll([&] { return host_writes != 1; }, 30));
    }
    host.local(u8"호스트에서 보낸 텍스트 😀\r\n둘째 줄");
    CHECK(poll([&] { auto s = read_clipboard(owner); return s && s->text == u8"호스트에서 보낸 텍스트 😀\r\n둘째 줄"; }));
    CHECK(!poll([&] { return host_writes != 1; }, 150)); // No echo from SetClipboardData.
    auto remote_sequence = GetClipboardSequenceNumber();
    host.local(u8"호스트에서 보낸 텍스트 😀\r\n둘째 줄");
    CHECK(!poll([&] { return GetClipboardSequenceNumber() != remote_sequence; }, 200));
    touch_metadata();
    CHECK(!poll([&] { return host_writes != 1; }, 150));
    for (int i = 0; i < 30; ++i) {
        auto text = "round " + std::to_string(i);
        if (i % 2) {
            host.local(text);
            CHECK(poll([&] { auto s = read_clipboard(owner); return s && s->text == text; }));
        } else { set_text(text); CHECK(poll([&] { return host_text == text; })); }
    }
    std::string unicode;
    for (int i=0; i<100000; ++i) unicode += u8"한글😀\r\n";
    CHECK(unicode.size() > 1024*1024);
    set_text(unicode); CHECK(poll([&] { return host_text == unicode; }, 15000));
    host.local(unicode);
    CHECK(poll([&] { auto s = read_clipboard(owner); return s && s->text == unicode; }, 15000));
    set_text(large); CHECK(poll([&] { return host_text == large; }, 60000));
    host.local(large);
    CHECK(poll([&] { auto s = read_clipboard(owner); return s && s->text == large; }, 60000));
    bridge.stop();
    auto final_text = *read_clipboard(owner)->text;
    CHECK(!bridge.receive(encode({Kind::text, {9999, 2}, 4, 0, "late"})));
    CHECK(*read_clipboard(owner)->text == final_text);
    CHECK(DestroyWindow(owner));
    CHECK(SetThreadDesktop(original_desktop));
    CHECK(SetProcessWindowStation(original_station));
    CloseDesktop(desktop); CloseWindowStation(station);
    std::cout << "PASS: isolated Windows clipboard, Unicode, locking, stale-write guard, worker bidirectional text, no initial copy, no echo, long Unicode, 16 MiB both ways, stop\n";
    return 0;
}

int main(int argc, char** argv) {
    std::ofstream output;
    if (argc == 2) {
        output.open(argv[1]);
        if (!output) return 2;
        std::cout.rdbuf(output.rdbuf());
        std::cerr.rdbuf(output.rdbuf());
        std::cout << std::unitbuf;
        std::cerr << std::unitbuf;
    }
    // A fresh thread has no inherited shell/IME windows that would prevent SetThreadDesktop.
    int result = 1;
    std::thread worker([&] { result = run_tests(); });
    worker.join();
    if (argc == 2) {
        std::cout.flush(); std::cerr.flush();
        std::cout.rdbuf(nullptr); std::cerr.rdbuf(nullptr);
    }
    return result;
}
