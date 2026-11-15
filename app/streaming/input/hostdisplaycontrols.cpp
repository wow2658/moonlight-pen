#include "hostdisplaycontrols.h"
#include <commctrl.h>
#include <windowsx.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <utility>

using namespace Gdiplus;
namespace {
constexpr UINT_PTR subclassId = 0x4D4C4443;
constexpr UINT menuId = 0x1F10;
constexpr wchar_t className[] = L"MoonlightHostDisplayPanelV2";
const Color background(255,35,38,44), foreground(255,239,242,247), muted(255,158,167,181);
constexpr COLORREF backgroundRgb = RGB(35,38,44);
HBRUSH backgroundBrush(HDC dc) {
    SetDCBrushColor(dc,backgroundRgb);
    return static_cast<HBRUSH>(GetStockObject(DC_BRUSH));
}
void rounded(Graphics& g,RectF r,float radius,Color fill,Color border=Color(0,0,0,0)) {
    GraphicsPath p;float d=radius*2;
    p.AddArc(r.X,r.Y,d,d,180,90);p.AddArc(r.GetRight()-d,r.Y,d,d,270,90);
    p.AddArc(r.GetRight()-d,r.GetBottom()-d,d,d,0,90);p.AddArc(r.X,r.GetBottom()-d,d,d,90,90);p.CloseFigure();
    SolidBrush brush(fill);g.FillPath(&brush,&p);
    if(border.GetA()){Pen pen(border,1);g.DrawPath(&pen,&p);}
}
void label(Graphics& g,const wchar_t* text,float x,float y,float w,float h,float size,Color color,
           bool bold=false,StringAlignment align=StringAlignmentNear) {
    Font font(L"Malgun Gothic",size,bold?FontStyleBold:FontStyleRegular,UnitPixel);
    SolidBrush brush(color);StringFormat format;
    format.SetAlignment(align);format.SetLineAlignment(StringAlignmentCenter);
    format.SetTrimming(StringTrimmingEllipsisCharacter);format.SetFormatFlags(StringFormatFlagsNoWrap);
    g.DrawString(text,-1,&font,RectF(x,y,w,h),&format,&brush);
}
void icon(Graphics& g,int action,float x,float y,Color color) {
    Pen pen(color,1.6f);
    if(action==HostDisplayControls::Monitor1||action==HostDisplayControls::Monitor2||action==HostDisplayControls::Toggle){
        g.DrawRectangle(&pen,x,y+1,18.0f,12.0f);g.DrawLine(&pen,x+9,y+13,x+9,y+17);g.DrawLine(&pen,x+5,y+17,x+13,y+17);
    }else if(action==HostDisplayControls::Clone){
        g.DrawRectangle(&pen,x,y,13.0f,12.0f);g.DrawRectangle(&pen,x+5,y+5,13.0f,12.0f);
    }else if(action==HostDisplayControls::Extend){
        g.DrawRectangle(&pen,x,y+2,8.0f,13.0f);g.DrawRectangle(&pen,x+11,y+2,8.0f,13.0f);
    }
}
}
HostDisplayControls::HostDisplayControls(HWND owner,bool showButton,std::function<void(Action)> callback,
                                         Placement placement,std::function<void(Placement)> save)
    :m_Owner(owner),m_ShowButton(showButton),m_Placement(placement),m_Callback(std::move(callback)),m_SavePlacement(std::move(save))
{
    GdiplusStartupInput startup;if(GdiplusStartup(&m_GdiToken,&startup,nullptr)!=Ok)return;
    WNDCLASSW wc={};wc.lpfnWndProc=windowProc;wc.hInstance=GetModuleHandleW(nullptr);
    wc.lpszClassName=className;wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.style=CS_DROPSHADOW;
    RegisterClassW(&wc);
    // The panel and buttons each paint through their own off-screen bitmap.
    // Avoid WS_EX_COMPOSITED: resized rounded popups can retain clipped pixels.
    m_Window=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,className,L"호스트 화면 제어",
        WS_POPUP|WS_CLIPCHILDREN,0,0,0,0,owner,nullptr,wc.hInstance,this);
    if(!m_Window)return;
    const wchar_t* names[]={L"화면 열기 / 접기",L"숨기기",L"모니터 1",L"모니터 2",L"복제",L"확장",L"필압 표시 켜기 / 끄기",L"호스트 도우미 연결 확인",L"위치와 패널 고정"};
    for(int i=0;i<9;++i){
        m_Buttons[i]=CreateWindowExW(0,L"BUTTON",names[i],WS_CHILD|BS_OWNERDRAW,0,0,0,0,m_Window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(i+1)),wc.hInstance,nullptr);
        SetWindowSubclass(m_Buttons[i],buttonProc,subclassId,reinterpret_cast<DWORD_PTR>(this));
    }
    SetWindowSubclass(owner,ownerProc,subclassId,reinterpret_cast<DWORD_PTR>(this));
    if(HMENU menu=GetSystemMenu(owner,FALSE))AppendMenuW(menu,MF_STRING,menuId,L"호스트 화면 제어 (Ctrl+Alt+Shift+B)");
    SetTimer(m_Window,1,250,nullptr);m_LastInside=GetTickCount64();layout();
}
HostDisplayControls::~HostDisplayControls(){
    if(m_Owner&&IsWindow(m_Owner)){
        RemoveWindowSubclass(m_Owner,ownerProc,subclassId);
        if(HMENU menu=GetSystemMenu(m_Owner,FALSE))DeleteMenu(menu,menuId,MF_BYCOMMAND);
    }
    if(m_Window&&IsWindow(m_Window))DestroyWindow(m_Window);
    if(m_GdiToken)GdiplusShutdown(m_GdiToken);
}
void HostDisplayControls::toggle(){m_Hidden=false;m_Expanded=!m_Expanded;m_LastInside=GetTickCount64();layout();}
void HostDisplayControls::collapse(bool hide){m_Expanded=false;if(hide)m_Hidden=true;layout();}
void HostDisplayControls::togglePin(){m_Placement.pinned=!m_Placement.pinned;savePlacement();InvalidateRect(m_Buttons[Pin-1],nullptr,FALSE);}
void HostDisplayControls::setPressureEnabled(bool enabled){m_PressureEnabled=enabled;if(enabled)m_Hidden=false;layout();}
void HostDisplayControls::updatePressure(const wchar_t* profile,float raw,float sent,int maxPercent){
    m_Profile=profile;m_HasPen=true;m_MaxPercent=maxPercent;
    m_Raw=std::isfinite(raw)?std::clamp(raw,0.0f,1.0f):0;m_Sent=std::isfinite(sent)?std::clamp(sent,0.0f,1.0f):0;
    if(m_PressureEnabled&&m_Window)InvalidateRect(m_Window,nullptr,FALSE);
}
void HostDisplayControls::savePlacement(){if(m_SavePlacement)m_SavePlacement(m_Placement);}
void HostDisplayControls::layout(){
    if(!m_Window||!m_Owner)return;
    RECT client;GetClientRect(m_Owner,&client);
    const int width=m_Expanded?320:m_PressureEnabled?260:100;
    const int height=m_Expanded?(m_PressureEnabled?404:322):(m_PressureEnabled?120:44);
    float scale=(GetDpiForWindow(m_Owner)?GetDpiForWindow(m_Owner):96)/96.0f;
    scale=(std::min)(scale,(std::max)(1L,client.right)/static_cast<float>(width));
    scale=(std::min)(scale,(std::max)(1L,client.bottom)/static_cast<float>(height));
    // Moving/focusing the owner does not change our layout. Re-showing every
    // child and resetting the region here caused unnecessary erase/paint cycles.
    if(m_LayoutValid&&width==m_Width&&height==m_Height&&scale==m_Scale){sync();return;}
    m_LayoutValid=true;m_Width=width;m_Height=height;m_Scale=scale;
    auto px=[this](int n){return static_cast<int>(std::lround(n*m_Scale));};
    auto button=[&](Action id,int x,int y,int w,int h,bool show){
        const bool wasVisible=(GetWindowLongPtrW(m_Buttons[id-1],GWL_STYLE)&WS_VISIBLE)!=0;
        if(wasVisible!=show)ShowWindow(m_Buttons[id-1],show?SW_SHOWNOACTIVATE:SW_HIDE);
        if(show)SetWindowPos(m_Buttons[id-1],nullptr,px(x),px(y),px(w),px(h),SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW);
    };
    button(Toggle,m_Expanded?272:0,m_Expanded?10:0,m_Expanded?32:m_Width,m_Expanded?32:44,true);
    button(Hide,0,0,0,0,false);button(Pin,232,10,32,32,m_Expanded);
    button(Monitor1,16,84,138,44,m_Expanded);button(Monitor2,166,84,138,44,m_Expanded);
    button(Clone,16,168,138,44,m_Expanded);button(Extend,166,168,138,44,m_Expanded);
    button(Pressure,16,240,288,36,m_Expanded);
    button(HostCheck,268,m_PressureEnabled?366:284,36,26,m_Expanded);
    SetWindowPos(m_Window,nullptr,0,0,(std::max)(1,px(m_Width)),(std::max)(1,px(m_Height)),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOCOPYBITS);
    SetWindowRgn(m_Window,CreateRoundRectRgn(0,0,px(m_Width)+1,px(m_Height)+1,px(24),px(24)),TRUE);
    sync();RedrawWindow(m_Window,nullptr,nullptr,RDW_INVALIDATE|RDW_FRAME|RDW_ALLCHILDREN);
}
void HostDisplayControls::sync(){
    if(!m_Owner||!m_Window)return;
    HWND active=GetForegroundWindow();
    bool visible=!m_Hidden&&(m_ShowButton||m_Expanded||m_PressureEnabled)&&IsWindowVisible(m_Owner)&&!IsIconic(m_Owner)&&
        (active==m_Owner||active==m_Window);
    if(!visible){if(IsWindowVisible(m_Window))ShowWindow(m_Window,SW_HIDE);return;}
    RECT rc,pr;GetClientRect(m_Owner,&rc);GetWindowRect(m_Window,&pr);POINT p={0,0};ClientToScreen(m_Owner,&p);
    float dpi=(GetDpiForWindow(m_Owner)?GetDpiForWindow(m_Owner):96)/96.0f;
    int maxX=static_cast<int>((std::max)(0L,rc.right-(pr.right-pr.left))),maxY=static_cast<int>((std::max)(0L,rc.bottom-(pr.bottom-pr.top)));
    int x=std::clamp(static_cast<int>(maxX-m_Placement.right*dpi),0,maxX),y=std::clamp(static_cast<int>(maxY-m_Placement.bottom*dpi),0,maxY);
    if(IsWindowVisible(m_Window)&&pr.left==p.x+x&&pr.top==p.y+y)return;
    SetWindowPos(m_Window,HWND_TOP,p.x+x,p.y+y,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_SHOWWINDOW);
}
void HostDisplayControls::paint(HDC dc){
    Graphics g(dc);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.ScaleTransform(m_Scale,m_Scale);g.Clear(background);
    if(m_Expanded){
        SolidBrush dot(muted);for(int x=0;x<2;x++)for(int y=0;y<3;y++)g.FillEllipse(&dot,18.0f+x*6,20.0f+y*6,2.5f,2.5f);
        label(g,L"화면 제어",42,10,180,34,17,foreground,true);
        label(g,L"모니터",16,57,270,23,13,muted);label(g,L"화면 모드",16,141,270,23,13,muted);
        Pen line(Color(255,61,66,76));g.DrawLine(&line,16,228,304,228);
        std::wstring footer=m_HasPen?m_Profile+L" · "+std::to_wstring(m_MaxPercent)+L"% → 100%":m_Profile;
        label(g,footer.c_str(),16,m_PressureEnabled?365:283,248,28,12,muted);
    }
    if(m_PressureEnabled){
        float y=m_Expanded?292.0f:48.0f;
        for(int i=0;i<2;i++){
            float value=i?m_Sent:m_Raw;Color accent=i?Color(255,111,216,179):Color(255,134,174,255);
            label(g,i?L"OUT":L"IN",16,y,38,24,11,accent,true);
            float width=m_Width-118.0f;rounded(g,RectF(60,y+7,width,10),5,Color(255,55,61,71));
            if(value>0)rounded(g,RectF(60,y+7,width*value,10),(std::min)(5.0f,width*value/2),accent);
            std::wstring number=std::to_wstring(static_cast<int>(std::lround(value*100)))+L"%";
            label(g,number.c_str(),m_Width-51.0f,y,36,24,12,foreground,false,StringAlignmentFar);y+=32;
        }
    }
}
void HostDisplayControls::paintButton(const DRAWITEMSTRUCT* item){
    // Button state changes must not clear the on-screen DC before drawing.
    const int width=item->rcItem.right-item->rcItem.left,height=item->rcItem.bottom-item->rcItem.top;
    if(width<=0||height<=0)return;
    HDC buffer=CreateCompatibleDC(item->hDC);
    HBITMAP bitmap=CreateCompatibleBitmap(item->hDC,width,height);
    if(!buffer||!bitmap){
        if(bitmap)DeleteObject(bitmap);
        if(buffer)DeleteDC(buffer);
        FillRect(item->hDC,&item->rcItem,backgroundBrush(item->hDC));
        return;
    }
    HGDIOBJ old=SelectObject(buffer,bitmap);
    DRAWITEMSTRUCT buffered=*item;
    buffered.hDC=buffer;buffered.rcItem={0,0,width,height};
    drawButton(&buffered);
    BitBlt(item->hDC,item->rcItem.left,item->rcItem.top,width,height,buffer,0,0,SRCCOPY);
    SelectObject(buffer,old);DeleteObject(bitmap);DeleteDC(buffer);
}
void HostDisplayControls::drawButton(const DRAWITEMSTRUCT* item){
    Graphics g(item->hDC);g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.ScaleTransform(m_Scale,m_Scale);g.Clear(background);
    int id=item->CtlID;float w=(item->rcItem.right-item->rcItem.left)/m_Scale,h=(item->rcItem.bottom-item->rcItem.top)/m_Scale;
    bool hover=m_HoverId==id,pressed=(item->itemState&ODS_SELECTED)!=0;
    if(id==Pressure){
        label(g,L"필압 표시",0,0,w-58,h,14,foreground);
        rounded(g,RectF(w-46,7,44,22),11,m_PressureEnabled?Color(255,105,190,159):Color(255,88,94,105));
        SolidBrush knob(Color(255,242,245,249));g.FillEllipse(&knob,w-(m_PressureEnabled?23.0f:43.0f),10.0f,16.0f,16.0f);return;
    }
    bool bordered=id==Monitor1||id==Monitor2||id==Clone||id==Extend;
    rounded(g,RectF(1,1,w-2,h-2),bordered?9:8,pressed?Color(255,65,74,88):hover?Color(255,51,58,70):background,bordered?Color(255,100,108,122):Color(0,0,0,0));
    if(bordered){
        icon(g,id,15,h/2-9,foreground);
        const wchar_t* text=id==Monitor1?L"모니터 1":id==Monitor2?L"모니터 2":id==Clone?L"복제":L"확장";
        label(g,text,43,0,w-48,h,14,foreground);return;
    }
    if(id==Toggle&&!m_Expanded){icon(g,Toggle,16,13,foreground);label(g,m_PressureEnabled?L"필압 · 화면":L"화면",44,0,w-52,h,14,foreground,true);return;}
    Pen pen(id==Pin&&m_Placement.pinned?Color(255,111,216,179):muted,1.7f);
    if(id==Toggle){g.DrawLine(&pen,9,12,16,19);g.DrawLine(&pen,16,19,23,12);}
    if(id==Pin){g.DrawRectangle(&pen,11.0f,7.0f,10.0f,11.0f);g.DrawLine(&pen,8,20,24,20);g.DrawLine(&pen,16,20,16,27);}
    if(id==HostCheck){g.DrawEllipse(&pen,10.0f,5.0f,16.0f,16.0f);label(g,L"?",10,3,16,20,12,muted,true,StringAlignmentCenter);}
}
LRESULT CALLBACK HostDisplayControls::windowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    auto self=reinterpret_cast<HostDisplayControls*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE){self=static_cast<HostDisplayControls*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(!self)return DefWindowProcW(hwnd,msg,wp,lp);
    switch(msg){
    case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
    case WM_ERASEBKGND:return 1;
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORSTATIC:{
        HDC dc=reinterpret_cast<HDC>(wp);
        SetBkColor(dc,backgroundRgb);SetTextColor(dc,RGB(239,242,247));
        return reinterpret_cast<LRESULT>(backgroundBrush(dc));}
    case WM_PAINT:{
        PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);RECT rc;GetClientRect(hwnd,&rc);
        HDC memory=CreateCompatibleDC(dc);HBITMAP bitmap=CreateCompatibleBitmap(dc,(std::max)(1L,rc.right),(std::max)(1L,rc.bottom));
        HGDIOBJ old=SelectObject(memory,bitmap);self->paint(memory);BitBlt(dc,0,0,rc.right,rc.bottom,memory,0,0,SRCCOPY);
        SelectObject(memory,old);DeleteObject(bitmap);DeleteDC(memory);EndPaint(hwnd,&ps);return 0;}
    case WM_DRAWITEM:self->paintButton(reinterpret_cast<DRAWITEMSTRUCT*>(lp));return TRUE;
    case WM_COMMAND:if(HIWORD(wp)==BN_CLICKED){if(self->m_Owner)SetFocus(self->m_Owner);self->m_LastInside=GetTickCount64();self->m_Callback(static_cast<Action>(LOWORD(wp)));}return 0;
    case WM_LBUTTONDOWN:
        if(self->m_Expanded&&!self->m_Placement.pinned&&GET_Y_LPARAM(lp)<52*self->m_Scale){
            self->m_Dragging=true;GetCursorPos(&self->m_DragStart);RECT r;GetWindowRect(hwnd,&r);self->m_DragOrigin={r.left,r.top};SetCapture(hwnd);
        }return 0;
    case WM_MOUSEMOVE:
        if(self->m_Dragging&&self->m_Owner){
            POINT cursor,origin={0,0};GetCursorPos(&cursor);ClientToScreen(self->m_Owner,&origin);RECT rc,pr;GetClientRect(self->m_Owner,&rc);GetWindowRect(hwnd,&pr);
            int maxX=origin.x+static_cast<int>((std::max)(0L,rc.right-(pr.right-pr.left))),maxY=origin.y+static_cast<int>((std::max)(0L,rc.bottom-(pr.bottom-pr.top)));
            int x=std::clamp(self->m_DragOrigin.x+cursor.x-self->m_DragStart.x,origin.x,static_cast<LONG>(maxX));
            int y=std::clamp(self->m_DragOrigin.y+cursor.y-self->m_DragStart.y,origin.y,static_cast<LONG>(maxY));
            float dpi=(GetDpiForWindow(self->m_Owner)?GetDpiForWindow(self->m_Owner):96)/96.0f;
            self->m_Placement.right=static_cast<int>((origin.x+rc.right-x-(pr.right-pr.left))/dpi);
            self->m_Placement.bottom=static_cast<int>((origin.y+rc.bottom-y-(pr.bottom-pr.top))/dpi);self->sync();
        }return 0;
    case WM_LBUTTONUP:if(self->m_Dragging){self->m_Dragging=false;ReleaseCapture();self->savePlacement();}return 0;
    case WM_CAPTURECHANGED:self->m_Dragging=false;return 0;
    case WM_TIMER:{
        if(self->m_Expanded&&!self->m_Placement.pinned&&!self->m_Dragging){
            POINT p;RECT rc;GetCursorPos(&p);GetWindowRect(hwnd,&rc);auto now=GetTickCount64();
            if(PtInRect(&rc,p))self->m_LastInside=now;
            else if(now-self->m_LastInside>2500){self->m_LastInside=now;self->m_Callback(Toggle);}
        }return 0;}
    case WM_CLOSE:self->m_Callback(Hide);return 0;
    case WM_NCDESTROY:self->m_Window=nullptr;break;
    }return DefWindowProcW(hwnd,msg,wp,lp);
}
LRESULT CALLBACK HostDisplayControls::buttonProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    auto self=reinterpret_cast<HostDisplayControls*>(data);
    if(msg==WM_ERASEBKGND){
        HDC dc=reinterpret_cast<HDC>(wp);RECT rc;GetClientRect(hwnd,&rc);
        FillRect(dc,&rc,backgroundBrush(dc));return 1;
    }
    if(msg==WM_MOUSEMOVE&&self->m_HoverId!=GetDlgCtrlID(hwnd)){
        const int previous=self->m_HoverId;self->m_HoverId=GetDlgCtrlID(hwnd);
        if(previous>0&&previous<=9)InvalidateRect(self->m_Buttons[previous-1],nullptr,FALSE);
        InvalidateRect(hwnd,nullptr,FALSE);TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,hwnd,0};TrackMouseEvent(&t);
    }
    if(msg==WM_MOUSELEAVE&&self->m_HoverId==GetDlgCtrlID(hwnd)){self->m_HoverId=0;InvalidateRect(hwnd,nullptr,FALSE);}
    if(msg==WM_NCDESTROY)RemoveWindowSubclass(hwnd,buttonProc,id);
    return DefSubclassProc(hwnd,msg,wp,lp);
}
LRESULT CALLBACK HostDisplayControls::ownerProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    auto self=reinterpret_cast<HostDisplayControls*>(data);
    if(msg==WM_SYSCOMMAND&&(wp&0xFFF0)==menuId){self->m_Callback(Toggle);return 0;}
    if(msg==WM_NCDESTROY){self->m_Owner=nullptr;RemoveWindowSubclass(hwnd,ownerProc,id);}
    LRESULT result=DefSubclassProc(hwnd,msg,wp,lp);
    if(msg==WM_WINDOWPOSCHANGED||msg==WM_DPICHANGED)self->layout();
    else if(msg==WM_ACTIVATE||msg==WM_ACTIVATEAPP)self->sync();
    return result;
}
