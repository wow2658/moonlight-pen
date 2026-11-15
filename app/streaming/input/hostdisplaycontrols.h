#pragma once
#include <windows.h>
#include <functional>
#include <string>

// Modeless panel: never blocks the SDL event/render loop.
class HostDisplayControls {
public:
    enum Action { Toggle = 1, Hide, Monitor1, Monitor2, Clone, Extend, Pressure, HostCheck, Pin };
    struct Placement { int right = 16; int bottom = 16; bool pinned = false; };
    HostDisplayControls(HWND owner, bool showButton, std::function<void(Action)> callback,
                        Placement placement, std::function<void(Placement)> savePlacement);
    ~HostDisplayControls();
    void toggle();
    void collapse(bool hide = false);
    bool expanded() const { return m_Expanded; }
    bool pinned() const { return m_Placement.pinned; }
    void togglePin();
    void setPressureEnabled(bool enabled);
    void updatePressure(const wchar_t* profile, float raw, float sent, int maxPercent);
private:
    static LRESULT CALLBACK windowProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK ownerProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
    static LRESULT CALLBACK buttonProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
    void layout();
    void sync();
    void paint(HDC dc);
    void paintButton(const DRAWITEMSTRUCT* item);
    void drawButton(const DRAWITEMSTRUCT* item);
    void savePlacement();
    HWND m_Owner;
    HWND m_Window = nullptr;
    HWND m_Buttons[9] = {};
    ULONG_PTR m_GdiToken = 0;
    bool m_Expanded = false, m_ShowButton, m_Hidden = false, m_PressureEnabled = false;
    bool m_Dragging = false;
    POINT m_DragStart = {}, m_DragOrigin = {};
    int m_HoverId = 0;
    float m_Scale = 1.0f, m_Raw = 0, m_Sent = 0;
    int m_Width = 100, m_Height = 44, m_MaxPercent = 70;
    std::wstring m_Profile = L"펜 입력 대기";
    bool m_HasPen = false;
    bool m_LayoutValid = false;
    ULONGLONG m_LastInside = 0;
    Placement m_Placement;
    std::function<void(Action)> m_Callback;
    std::function<void(Placement)> m_SavePlacement;
};
