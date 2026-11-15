#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>
#include "../app/streaming/input/hostdisplayprotocol.h"
#include "displayrequest.h"

namespace {
constexpr UINT TrayMessage = WM_APP + 1;
NOTIFYICONDATAW tray = {};
HWND helperWindow = nullptr;
bool busy = false;
std::filesystem::path logPath;

void log(const std::wstring& message) {
    if(logPath.empty())return;
    SYSTEMTIME now;GetLocalTime(&now);
    std::wofstream stream(logPath,std::ios::app);
    stream<<now.wYear<<L'-'<<now.wMonth<<L'-'<<now.wDay<<L' '<<now.wHour<<L':'<<now.wMinute<<L':'<<now.wSecond<<L" "<<message<<L'\n';
}
void notify(const wchar_t* title,const std::wstring& message,bool error=false) {
    log(std::wstring(title)+L": "+message);
    tray.uFlags=NIF_INFO;
    wcsncpy_s(tray.szInfoTitle,title,_TRUNCATE);wcsncpy_s(tray.szInfo,message.c_str(),_TRUNCATE);
    tray.dwInfoFlags=error?NIIF_ERROR:NIIF_INFO;Shell_NotifyIconW(NIM_MODIFY,&tray);
}
std::wstring errorText(LONG code) {
    wchar_t* text=nullptr;FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,code,0,reinterpret_cast<wchar_t*>(&text),0,nullptr);
    std::wstring result=std::to_wstring(code)+L": "+(text?text:L"알 수 없는 오류");
    if(text)LocalFree(text);return result;
}
struct State { LONG error = ERROR_GEN_FAILURE; unsigned monitors = 0, sources = 0; };
State readState() {
    for(int retry=0;retry<3;retry++){
        UINT32 pathCount=0,modeCount=0;
        LONG result=GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS,&pathCount,&modeCount);
        if(result!=ERROR_SUCCESS)return {result,0,0};
        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
        result=QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS,&pathCount,paths.data(),&modeCount,modes.data(),nullptr);
        if(result==ERROR_INSUFFICIENT_BUFFER)continue;
        if(result!=ERROR_SUCCESS)return {result,0,0};
        std::set<std::pair<unsigned long long,unsigned>> sources;
        for(UINT32 i=0;i<pathCount;i++){
            auto& s=paths[i].sourceInfo;
            unsigned long long adapter=(static_cast<unsigned long long>(static_cast<unsigned>(s.adapterId.HighPart))<<32)|s.adapterId.LowPart;
            sources.insert({adapter,s.id});
        }
        return {ERROR_SUCCESS,pathCount,static_cast<unsigned>(sources.size())};
    }
    return {ERROR_INSUFFICIENT_BUFFER,0,0};
}
std::wstring stateText(const State& state) {
    if(state.error!=ERROR_SUCCESS)return L"상태 조회 실패: "+errorText(state.error);
    if(state.monitors<2)return L"활성 모니터 "+std::to_wstring(state.monitors)+L"개";
    if(state.sources==1)return L"복제 · 활성 모니터 "+std::to_wstring(state.monitors)+L"개";
    if(state.sources==state.monitors)return L"확장 · 활성 모니터 "+std::to_wstring(state.monitors)+L"개";
    return L"복제/확장 혼합 구성";
}
void request(unsigned id) {
    if(id==HostDisplayProtocol::ProbeId){
        notify(L"문라이트 호스트 도우미 연결 확인",L"명령을 받았습니다. 현재 "+stateText(readState()));return;
    }
    if(busy)return;
    busy=true;
    unsigned topology=id==HostDisplayProtocol::CloneId?DisplayRequest::Clone:DisplayRequest::Extend;
    LONG result=DisplayRequest::apply(topology,[](unsigned flags){return SetDisplayConfig(0,nullptr,0,nullptr,flags);});
    if(result!=ERROR_SUCCESS)notify(L"화면 모드 변경 실패",errorText(result),true);
    else {
        // Verify after the driver settles, outside a blocking UI loop.
        SetTimer(helperWindow,10+id,800,nullptr);
        return;
    }
    busy=false;
}
void registerKeys(HWND hwnd) {
    constexpr UINT mods=MOD_CONTROL|MOD_ALT|MOD_SHIFT|MOD_NOREPEAT;
    bool ok=true;
    ok=RegisterHotKey(hwnd,HostDisplayProtocol::CloneId,mods,HostDisplayProtocol::CloneKey)&&ok;
    ok=RegisterHotKey(hwnd,HostDisplayProtocol::ExtendId,mods,HostDisplayProtocol::ExtendKey)&&ok;
    ok=RegisterHotKey(hwnd,HostDisplayProtocol::ProbeId,mods,HostDisplayProtocol::ProbeKey)&&ok;
    if(!ok)notify(L"도우미 단축키 충돌",L"Ctrl+Alt+Shift+D/E/H를 사용하는 프로그램을 확인하세요.",true);
    else notify(L"문라이트 화면 도우미 준비 완료",L"복제/확장 버튼을 사용할 수 있습니다. 문라이트의 ? 버튼으로 연결을 확인하세요.");
}
LRESULT CALLBACK windowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_HOTKEY){request(static_cast<unsigned>(wp));return 0;}
    if(msg==WM_TIMER){
        KillTimer(hwnd,wp);State state=readState();
        bool clone=wp==10+HostDisplayProtocol::CloneId;
        bool matches=state.error==ERROR_SUCCESS&&state.monitors>=2&&(clone?state.sources==1:state.sources==state.monitors);
        notify(matches?L"화면 모드 적용 완료":L"화면 모드 확인 필요",
            std::wstring(clone?L"복제 요청 · ":L"확장 요청 · ")+stateText(state),!matches);
        busy=false;return 0;
    }
    if(msg==TrayMessage){
        if(LOWORD(lp)==WM_LBUTTONUP){request(HostDisplayProtocol::ProbeId);return 0;}
        if(LOWORD(lp)==WM_RBUTTONUP||LOWORD(lp)==WM_CONTEXTMENU){
            HMENU menu=CreatePopupMenu();
            AppendMenuW(menu,MF_STRING,1,L"복제");AppendMenuW(menu,MF_STRING,2,L"확장");
            AppendMenuW(menu,MF_STRING,3,L"상태 확인");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);AppendMenuW(menu,MF_STRING,4,L"도우미 종료");
            POINT p;GetCursorPos(&p);SetForegroundWindow(hwnd);
            UINT action=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,p.x,p.y,0,hwnd,nullptr);
            DestroyMenu(menu);
            if(action==4)DestroyWindow(hwnd);else if(action)request(action);
            return 0;
        }
    }
    if(msg==WM_DESTROY){
        for(int i=1;i<=3;++i)UnregisterHotKey(hwnd,i);
        Shell_NotifyIconW(NIM_DELETE,&tray);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,msg,wp,lp);
}
int selfTest() {
    std::vector<unsigned> calls;
    auto result=DisplayRequest::apply(DisplayRequest::Clone,[&](unsigned flags){calls.push_back(flags);return 0L;});
    if(result!=0||calls!=std::vector<unsigned>{0x42,0x82})return 1;
    calls.clear();
    result=DisplayRequest::apply(DisplayRequest::Extend,[&](unsigned flags){calls.push_back(flags);return 5L;});
    if(result!=5||calls!=std::vector<unsigned>{0x44})return 2;
    calls.clear();
    result=DisplayRequest::apply(DisplayRequest::Extend,[&](unsigned flags){calls.push_back(flags);return flags&0x80?31L:0L;});
    if(result!=31||calls!=std::vector<unsigned>{0x44,0x84})return 3;
    if(HostDisplayProtocol::CloneKey!=0x44||HostDisplayProtocol::ExtendKey!=0x45||HostDisplayProtocol::ProbeKey!=0x48)return 4;
    calls.clear();
    if(DisplayRequest::apply(0,[&](unsigned f){calls.push_back(f);return 0L;})!=87||!calls.empty())return 5;
    return 0;
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR args,int) {
    if(std::wstring(args)==L"--self-test")return selfTest();
    if(std::wstring(args)==L"--stop"){
        if(HWND w=FindWindowW(L"MoonlightDisplayHelperV1",nullptr))PostMessageW(w,WM_CLOSE,0,0);
        return 0;
    }
    HANDLE singleton=CreateMutexW(nullptr,FALSE,L"Local\\MoonlightDisplayHelperV1");
    if(GetLastError()==ERROR_ALREADY_EXISTS){if(singleton)CloseHandle(singleton);return 0;}
    PWSTR local=nullptr;
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local))){
        std::filesystem::path directory=std::filesystem::path(local)/L"MoonlightDisplayHelper";
        CoTaskMemFree(local);std::error_code ec;std::filesystem::create_directories(directory,ec);
        logPath=directory/L"helper.log";
        if(std::filesystem::file_size(logPath,ec)>1024*1024&&!ec){std::wofstream clear(logPath);}
    }
    WNDCLASSW wc={};wc.lpfnWndProc=windowProc;wc.hInstance=instance;wc.lpszClassName=L"MoonlightDisplayHelperV1";RegisterClassW(&wc);
    helperWindow=CreateWindowW(wc.lpszClassName,L"Moonlight Display Helper",0,0,0,0,0,nullptr,nullptr,instance,nullptr);
    if(!helperWindow)return 6;
    tray.cbSize=sizeof(tray);tray.hWnd=helperWindow;tray.uID=1;tray.uCallbackMessage=TrayMessage;
    tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;tray.hIcon=LoadIcon(nullptr,IDI_APPLICATION);
    wcscpy_s(tray.szTip,L"Moonlight 화면 도우미");Shell_NotifyIconW(NIM_ADD,&tray);
    tray.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&tray);
    registerKeys(helperWindow);
    MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}
    if(singleton)CloseHandle(singleton);return 0;
}
