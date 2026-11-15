// Exercise only windows created by this test, without input injection or a host.
#define NOMINMAX
#include "../app/streaming/input/hostdisplaycontrols.h"
#include <windowsx.h>
#include <cassert>
#include <cstdio>
#include <memory>

struct Surface {
    HDC dc;
    HBITMAP bitmap;
    HGDIOBJ previous;
    unsigned* pixels;
    int width, height;
    Surface(int w,int h):width(w),height(h) {
        dc=CreateCompatibleDC(nullptr);
        BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,reinterpret_cast<void**>(&pixels),nullptr,0);
        assert(bitmap&&dc);previous=SelectObject(dc,bitmap);white();
    }
    void white(){GdiFlush();for(int i=0;i<width*height;i++)pixels[i]=0x00FFFFFF;}
    void checkDarkBackground(){GdiFlush();for(int i=0;i<width*height;i++)assert((pixels[i]&0xFFFFFF)==0x23262C);}
    void checkNoWhite(){GdiFlush();for(int i=0;i<width*height;i++)assert((pixels[i]&0xFFFFFF)!=0xFFFFFF);}
    ~Surface(){SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);}
};
struct FindPopup { HWND owner; HWND panel=nullptr; };
BOOL CALLBACK findPopup(HWND hwnd,LPARAM data) {
    auto state=reinterpret_cast<FindPopup*>(data);
    wchar_t name[128]={};GetClassNameW(hwnd,name,128);
    if(GetWindow(hwnd,GW_OWNER)==state->owner&&wcscmp(name,L"MoonlightHostDisplayPanelV2")==0)state->panel=hwnd;
    return TRUE;
}
int main() {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW cls={};cls.lpfnWndProc=DefWindowProcW;cls.hInstance=GetModuleHandleW(nullptr);
    cls.lpszClassName=L"HostPanelPaintRegressionOwner";RegisterClassW(&cls);
    HWND owner=CreateWindowW(cls.lpszClassName,L"Paint regression",WS_OVERLAPPEDWINDOW,0,0,1000,700,nullptr,nullptr,cls.hInstance,nullptr);
    assert(owner);
    auto panel=std::make_unique<HostDisplayControls>(owner,true,[](HostDisplayControls::Action){},
        HostDisplayControls::Placement{},[](HostDisplayControls::Placement){});
    panel->toggle();panel->setPressureEnabled(true);panel->updatePressure(L"S펜",0.35f,0.7071f,70);
    FindPopup found{owner};EnumThreadWindows(GetCurrentThreadId(),findPopup,reinterpret_cast<LPARAM>(&found));assert(found.panel);
    for(int id : {1,3,4,5,6,7,8,9}) {
        HWND button=GetDlgItem(found.panel,id);assert(button);
        RECT rc;GetClientRect(button,&rc);assert(rc.right>0&&rc.bottom>0);
        Surface surface(rc.right,rc.bottom);
        assert(SendMessageW(button,WM_ERASEBKGND,reinterpret_cast<WPARAM>(surface.dc),0)==1);
        surface.checkDarkBackground();
        for(unsigned state : {0U,static_cast<unsigned>(ODS_SELECTED),static_cast<unsigned>(ODS_FOCUS)}) {
            surface.white();
            DRAWITEMSTRUCT draw={};draw.CtlType=ODT_BUTTON;draw.CtlID=id;draw.itemAction=ODA_DRAWENTIRE;
            draw.itemState=state;draw.hwndItem=button;draw.hDC=surface.dc;draw.rcItem=rc;
            assert(SendMessageW(found.panel,WM_DRAWITEM,id,reinterpret_cast<LPARAM>(&draw)));
            surface.checkNoWhite();
        }
        SendMessageW(button,WM_MOUSEMOVE,0,MAKELPARAM(2,2));ValidateRect(button,nullptr);
        assert(!GetUpdateRect(button,nullptr,FALSE));
        for(int i=0;i<50;i++)SendMessageW(button,WM_MOUSEMOVE,0,MAKELPARAM(3,3));
        assert(!GetUpdateRect(button,nullptr,FALSE)); // no repaint storm while hovering
        SendMessageW(button,WM_MOUSELEAVE,0,0);
    }
    panel->collapse();panel->toggle();panel->setPressureEnabled(false);
    panel.reset();DestroyWindow(owner);
    puts("PASS: dark erase; buffered normal/pressed/focus buttons contain no white; stable hover; expand/collapse.");
}
