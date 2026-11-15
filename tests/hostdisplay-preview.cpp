// Local UI harness: records actions, never sends input to a host or changes displays.
#include "../app/streaming/input/hostdisplaycontrols.h"
#include <memory>
std::unique_ptr<HostDisplayControls> panel;
bool pressureEnabled = false;
LRESULT CALLBACK previewProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_KEYDOWN && wp == 'B' && panel) { panel->toggle(); return 0; }
    if (msg == WM_DESTROY) { PostQuitMessage(0); return 0; }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR args, int)
{
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW wc = {};
    wc.lpfnWndProc = previewProc; wc.hInstance = instance;
    wc.lpszClassName = L"MoonlightDisplayPreview";
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);
    HWND owner = CreateWindowW(wc.lpszClassName, L"화면 제어 테스트 (호스트에 전송하지 않음)",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 1000, 560, nullptr, nullptr, instance, nullptr);
    panel = std::make_unique<HostDisplayControls>(owner, true, [owner](HostDisplayControls::Action action) {
        if (action == HostDisplayControls::Toggle) panel->toggle();
        else if (action == HostDisplayControls::Pin) panel->togglePin();
        else if (action == HostDisplayControls::Pressure) {
            pressureEnabled = !pressureEnabled;
            panel->setPressureEnabled(pressureEnabled);
            panel->updatePressure(L"S펜", 0.35f, 0.7071f, 70);
        }
        else {
            const wchar_t* result = action == HostDisplayControls::Monitor1 ? L"모니터 1 선택 — 테스트 통과" :
                action == HostDisplayControls::Monitor2 ? L"모니터 2 선택 — 테스트 통과" :
                action == HostDisplayControls::Clone ? L"복제 선택 — 테스트 통과" :
                action == HostDisplayControls::Extend ? L"확장 선택 — 테스트 통과" : L"호스트 연결 확인 — 테스트";
            SetWindowTextW(owner, result);
            panel->collapse(action == HostDisplayControls::Hide);
        }
    }, HostDisplayControls::Placement{16,16,true}, [](HostDisplayControls::Placement) {});
    if (std::wstring(args) == L"--expanded") panel->toggle();
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    panel.reset();
    return 0;
}
