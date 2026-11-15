// This receiver NEVER changes a device. It starts the original, hash-pinned one-shot helper.
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cfgmgr32.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <stdexcept>
#ifdef MOONLIGHT_PUBLIC_TOUCH
#include "verified-devices.h"
#include "backend-pin.h"
#else
#include "device-profile.local.h"
#endif
#include <algorithm>

namespace {
constexpr wchar_t backend[] = L"C:\\Program Files\\Laptop Finger Touch Toggle\\TouchToggle.exe";
constexpr wchar_t workingDirectory[] = L"C:\\Program Files\\Laptop Finger Touch Toggle";
constexpr char expectedHash[] = MOONLIGHT_BACKEND_SHA256;
#ifdef MOONLIGHT_PUBLIC_TOUCH
VerifiedTouch::Profile deviceProfile;
const wchar_t* touchId = nullptr;
const wchar_t* penId = nullptr;
#else
constexpr wchar_t touchId[] = MOONLIGHT_TOUCH_ID;
constexpr wchar_t penId[] = MOONLIGHT_PEN_ID;
#endif
constexpr UINT hotkeyId = 0x6B10;
bool keepRunning = false;
HWND banner = nullptr;
void require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
struct Handle {
    HANDLE value = nullptr;
    explicit Handle(HANDLE h = nullptr):value(h){}
    ~Handle(){if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
    void close(){if(value && value!=INVALID_HANDLE_VALUE)CloseHandle(value);value=nullptr;}
    Handle(const Handle&)=delete;
    Handle& operator=(const Handle&)=delete;
};
void output(const std::string& text) {
    DWORD written=0; HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE);
    if(h && h!=INVALID_HANDLE_VALUE)WriteFile(h,text.data(),static_cast<DWORD>(text.size()),&written,nullptr);
}
void log(const std::string& text) {
    Handle file(CreateFileW(L"C:\\Program Files\\Laptop Finger Touch Toggle\\dispatcher-events.log",GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
    if(file.value==INVALID_HANDLE_VALUE)return;
    LARGE_INTEGER size={};GetFileSizeEx(file.value,&size);
    if(size.QuadPart>65536){SetFilePointer(file.value,0,nullptr,FILE_BEGIN);SetEndOfFile(file.value);}
    SetFilePointer(file.value,0,nullptr,FILE_END);
    SYSTEMTIME now;GetLocalTime(&now);
    char prefix[64];wsprintfA(prefix,"%04u-%02u-%02u %02u:%02u:%02u ",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond);
    const auto line=std::string(prefix)+text+"\r\n";DWORD written=0;
    WriteFile(file.value,line.data(),static_cast<DWORD>(line.size()),&written,nullptr);
}
void verify_backend() {
    Handle file(CreateFileW(backend,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
    require(file.value!=INVALID_HANDLE_VALUE,"Original backend is missing.");
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0,"Cannot initialize hash verification.");
    std::vector<unsigned char> object;
    struct HashGuard{BCRYPT_ALG_HANDLE* a;BCRYPT_HASH_HANDLE* h;~HashGuard(){if(*h)BCryptDestroyHash(*h);if(*a)BCryptCloseAlgorithmProvider(*a,0);}} guard{&algorithm,&hash};
    DWORD bytes=0,objectSize=0;
    require(BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&objectSize),sizeof(objectSize),&bytes,0)>=0,"Cannot size hash object.");
    object.resize(objectSize);
    require(BCryptCreateHash(algorithm,&hash,object.data(),objectSize,nullptr,0,0)>=0,"Cannot create hash.");
    unsigned char chunk[16384];DWORD read=0;
    for(;;){require(ReadFile(file.value,chunk,sizeof(chunk),&read,nullptr)!=FALSE,"Cannot read original backend.");if(!read)break;require(BCryptHashData(hash,chunk,read,0)>=0,"Cannot hash original backend.");}
    unsigned char digest[32];require(BCryptFinishHash(hash,digest,sizeof(digest),0)>=0,"Cannot finish hash.");
    std::string hex;const char* digits="0123456789abcdef";
    for(auto b:digest){hex.push_back(digits[b>>4]);hex.push_back(digits[b&15]);}
    require(hex==expectedHash,"Original backend changed; refusing to run an unverified replacement.");
}
ULONG problem(const wchar_t* id) {
    DEVINST node=0;ULONG flags=0,code=0;
    require(CM_Locate_DevNodeW(&node,const_cast<wchar_t*>(id),CM_LOCATE_DEVNODE_NORMAL)==CR_SUCCESS &&
            CM_Get_DevNode_Status(&flags,&code,node,0)==CR_SUCCESS,"Cannot inspect the expected device.");
    return code;
}
struct State { bool on; bool coherent; bool penHealthy; int contacts; };
State inspect() {
    const auto code=problem(touchId);
    require(code==0 || code==CM_PROB_DISABLED,"Touch device reports an unrelated problem.");
    bool pointerTouch=false,targetRawTouch=false;
    UINT32 count=0;
    require(GetPointerDevices(&count,nullptr)!=FALSE,"Cannot inspect pointer device registration.");
    std::vector<POINTER_DEVICE_INFO> pointers(count);
    require(!count || GetPointerDevices(&count,pointers.data())!=FALSE,"Cannot enumerate pointer devices.");
    for(const auto& item:pointers)if(item.pointerDeviceType==POINTER_DEVICE_TYPE_TOUCH)pointerTouch=true;
    UINT rawCount=0;
    require(GetRawInputDeviceList(nullptr,&rawCount,sizeof(RAWINPUTDEVICELIST))==0,"Cannot size raw input device list.");
    std::vector<RAWINPUTDEVICELIST> raw(rawCount);
    require(GetRawInputDeviceList(raw.data(),&rawCount,sizeof(RAWINPUTDEVICELIST))!=UINT(-1),"Cannot enumerate raw input devices.");
    for(const auto& item:raw) {
        RID_DEVICE_INFO info={};info.cbSize=sizeof(info);UINT size=sizeof(info);
        if(GetRawInputDeviceInfoW(item.hDevice,RIDI_DEVICEINFO,&info,&size)==UINT(-1) || info.dwType!=RIM_TYPEHID || info.hid.usUsagePage!=0x0d || info.hid.usUsage!=4)continue;
        wchar_t name[1024]={};size=1024;
        if(GetRawInputDeviceInfoW(item.hDevice,RIDI_DEVICENAME,name,&size)==UINT(-1))continue;
        std::wstring path(name);std::transform(path.begin(),path.end(),path.begin(),[](wchar_t c){return static_cast<wchar_t>(towupper(c));});
        if(path.find(L"STMD1234&COL01")!=std::wstring::npos)targetRawTouch=true;
    }
    const bool on=code==0;
    const bool stackOn=pointerTouch && targetRawTouch && (GetSystemMetrics(SM_DIGITIZER)&NID_INTEGRATED_TOUCH)!=0;
    const bool stackOff=!pointerTouch && !targetRawTouch;
    return {on,on?stackOn:stackOff,problem(penId)==0,GetSystemMetrics(SM_MAXIMUMTOUCHES)};
}
std::string describe(const State& state) {
    return std::string("touch=")+(state.on?"on":"off")+" coherent="+(state.coherent?"yes":"no")+
        " pen="+(state.penHealthy?"healthy":"problem")+" contacts="+std::to_string(state.contacts);
}
struct ChildResult{DWORD exitCode;std::string text;ULONGLONG elapsed;};
ChildResult run_backend(const std::wstring& operation) {
    require(operation==L"--status" || operation==L"--toggle" || operation==L"--on" || operation==L"--off","Invalid fixed backend command.");
    verify_backend();
    SECURITY_ATTRIBUTES security={sizeof(security),nullptr,TRUE};HANDLE readRaw=nullptr,writeRaw=nullptr;
    require(CreatePipe(&readRaw,&writeRaw,&security,0)!=FALSE,"Cannot create status pipe.");
    Handle read(readRaw),write(writeRaw);
    require(SetHandleInformation(read.value,HANDLE_FLAG_INHERIT,0)!=FALSE,"Cannot protect status pipe.");
    Handle input(CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
    require(input.value!=INVALID_HANDLE_VALUE,"Cannot open empty child input.");
    SIZE_T attributeSize=0;InitializeProcThreadAttributeList(nullptr,1,0,&attributeSize);
    std::vector<unsigned char> storage(attributeSize);
    auto attributes=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    require(InitializeProcThreadAttributeList(attributes,1,0,&attributeSize)!=FALSE,"Cannot initialize restricted child handles.");
    struct AttributesGuard{LPPROC_THREAD_ATTRIBUTE_LIST p;~AttributesGuard(){DeleteProcThreadAttributeList(p);}} guard{attributes};
    HANDLE inherited[]={write.value,input.value};
    require(UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr)!=FALSE,"Cannot restrict inherited child handles.");
    STARTUPINFOEXW startup={};startup.StartupInfo.cb=sizeof(startup);startup.lpAttributeList=attributes;
    startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;startup.StartupInfo.hStdInput=input.value;startup.StartupInfo.hStdOutput=write.value;startup.StartupInfo.hStdError=write.value;
    std::wstring command=L"\""+std::wstring(backend)+L"\" "+operation+L" --quiet";
    PROCESS_INFORMATION process={};const auto start=GetTickCount64();
    require(CreateProcessW(backend,command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,workingDirectory,&startup.StartupInfo,&process)!=FALSE,"Original backend could not start.");
    Handle child(process.hProcess),thread(process.hThread);write.close();
    // Never terminate a child in the middle of a hardware transition.
    require(WaitForSingleObject(child.value,10000)==WAIT_OBJECT_0,"Device transition is still running; automatic toggling has been suspended.");
    DWORD exitCode=0;require(GetExitCodeProcess(child.value,&exitCode)!=FALSE,"Cannot inspect backend completion.");
    std::string text;char buffer[256];DWORD received=0;
    while(ReadFile(read.value,buffer,sizeof(buffer),&received,nullptr) && received){require(text.size()+received<=2048,"Unexpected backend status size.");text.append(buffer,received);}
    return {exitCode,text,GetTickCount64()-start};
}
LRESULT CALLBACK banner_proc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(window,&ps);RECT r;GetClientRect(window,&r);HBRUSH brush=CreateSolidBrush(RGB(35,38,44));FillRect(dc,&r,brush);DeleteObject(brush);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(240,242,245));HFONT font=CreateFontW(-19,0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Malgun Gothic");auto old=SelectObject(dc,font);wchar_t text[192];GetWindowTextW(window,text,192);DrawTextW(dc,text,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);SelectObject(dc,old);DeleteObject(font);EndPaint(window,&ps);return 0;}
    if(message==WM_TIMER){DestroyWindow(window);return 0;}
    if(message==WM_DESTROY){if(banner==window)banner=nullptr;if(!keepRunning)PostQuitMessage(0);return 0;}
    return DefWindowProcW(window,message,wp,lp);
}
void notice(const wchar_t* text) {
    if(banner)DestroyWindow(banner);
    WNDCLASSW wc={};wc.lpfnWndProc=banner_proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"FingerTouchDispatcherNotice";RegisterClassW(&wc);
    POINT point;GetCursorPos(&point);MONITORINFO monitor={};monitor.cbSize=sizeof(monitor);GetMonitorInfoW(MonitorFromPoint(point,MONITOR_DEFAULTTOPRIMARY),&monitor);
    banner=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,wc.lpszClassName,text,WS_POPUP,monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-480)/2,monitor.rcWork.top+36,480,58,nullptr,nullptr,wc.hInstance,nullptr);
    if(banner){ShowWindow(banner,SW_SHOWNOACTIVATE);UpdateWindow(banner);SetTimer(banner,1,1400,nullptr);}
}
State transition(const std::wstring& operation) {
    const auto before=inspect();require(before.coherent && before.penHealthy,"Device/input state is inconsistent; no automatic change was requested.");
    const bool target=operation==L"--on" || (operation==L"--toggle" && !before.on);
    const auto result=run_backend(operation);
    log("backend_exit="+std::to_string(result.exitCode)+" backend_ms="+std::to_string(result.elapsed));
    require(result.exitCode==0,"The original backend did not complete successfully.");
    require(result.text.find(target?"touch=enabled":"touch=disabled")!=std::string::npos,"The backend did not report the requested state.");
    State state=inspect();unsigned stable=0;
    const auto start=GetTickCount64();
    while(GetTickCount64()-start<2500){state=inspect();stable=(state.on==target && state.coherent && state.penHealthy)?stable+1:0;if(stable>=3){log("verified "+describe(state));return state;}Sleep(100);}
    log("unverified "+describe(state));
    throw std::runtime_error("Windows input registration did not match the requested state; automatic toggling is suspended.");
}
}
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
    try {
#ifdef MOONLIGHT_PUBLIC_TOUCH
        require(SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32) != FALSE,"Cannot restrict DLL loading.");
        deviceProfile = VerifiedTouch::discover(); touchId = deviceProfile.touch.c_str(); penId = deviceProfile.pen.c_str();
#endif
        int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);require(argv && argc==2,"Specify exactly one supported mode.");std::wstring mode=argv[1];LocalFree(argv);
        verify_backend();
        if(mode==L"--inspect"){const auto state=inspect();output(describe(state)+"\n");return state.coherent&&state.penHealthy?0:2;}
        if(mode==L"--transport-test"){const auto result=run_backend(L"--status");require(result.exitCode==0,"Read-only child transport failed.");output("Original backend SHA256 verified.\n"+result.text);return 0;}
        if(mode==L"--off-once" || mode==L"--on-once"){output(describe(transition(mode==L"--off-once"?L"--off":L"--on"))+"\n");return 0;}
        require(mode==L"--listen","Unsupported dispatcher mode.");
        Handle singleton(CreateMutexW(nullptr,FALSE,L"Local\\Samsung940X3MSeparateTouchDispatcher"));require(singleton.value!=nullptr,"Cannot initialize dispatcher lock.");
        const auto held=WaitForSingleObject(singleton.value,0);if(held!=WAIT_OBJECT_0 && held!=WAIT_ABANDONED)return 0;
        struct MutexGuard{HANDLE value;~MutexGuard(){ReleaseMutex(value);}} mutexGuard{singleton.value};
        const auto initial=inspect();require(initial.coherent&&initial.penHealthy,"Initial input state is not healthy.");
        require(RegisterHotKey(nullptr,hotkeyId,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,VK_F10)!=FALSE,"Ctrl+Alt+F10 is already registered.");
        struct KeyGuard{~KeyGuard(){UnregisterHotKey(nullptr,hotkeyId);}} keyGuard;
        keepRunning=true;bool suspended=false;ULONGLONG completed=0;log("listener_started "+describe(initial));
        MSG message;
        while(GetMessageW(&message,nullptr,0,0)>0){
            if(message.message==WM_HOTKEY && message.wParam==hotkeyId){
                if(suspended){notice(L"장치 상태 확인이 필요합니다 · 자동 전환 중지");continue;}
                if(completed && GetTickCount64()-completed<500)continue;
                try{const auto state=transition(L"--toggle");notice(state.on?L"손터치 장치 켜짐 · 실제 터치를 확인해 주세요":L"손터치 장치 꺼짐 · S펜 유지");}
                catch(const std::exception& error){suspended=true;log(std::string("suspended ")+error.what());notice(L"입력 상태 불일치 · 자동 전환을 중지했습니다");}
                completed=GetTickCount64();MSG queued;while(PeekMessageW(&queued,nullptr,WM_HOTKEY,WM_HOTKEY,PM_REMOVE)){}
            }else{TranslateMessage(&message);DispatchMessageW(&message);}
        }
        if(banner)DestroyWindow(banner);return 0;
    }catch(const std::exception& error){log(std::string("error ")+error.what());output(std::string("ERROR: ")+error.what()+"\n");return 1;}
}
