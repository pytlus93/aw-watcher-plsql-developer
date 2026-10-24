#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <set>

static HWND host=nullptr,child=nullptr;
static DWORD ownerThread=0;
static std::atomic<int> wrongThread{0},callbackCalls{0};
static int assertions=0;
static std::set<std::filesystem::path> baseline;
static void check(bool b,const char* reason) {++assertions;if(!b) throw std::runtime_error(reason);}
static void affinity() {++callbackCalls;if(GetCurrentThreadId()!=ownerThread) ++wrongThread;}
static int __cdecl version(){affinity();return 1600;}
static int __cdecl type(){affinity();return 3;}
static HWND __cdecl mainHandle(){affinity();return host;}
static HWND __cdecl childHandle(){affinity();return child;}
static char* __cdecl file(){affinity();return const_cast<char*>("C:\\synthetic\\example.sql");}
static int __cdecl connectionIndex(){affinity();return 0;}
static BOOL __cdecl object(char** t,char** o,char** n,char** s) {
    affinity();*t=const_cast<char*>("PACKAGE");*o=const_cast<char*>("SYNTHETIC_OWNER");*n=const_cast<char*>("SYNTHETIC_OBJECT");*s=nullptr;return TRUE;
}
static BOOL __cdecl connection(int,char** u,char** p,char** d,char** a) {
    affinity();*u=reinterpret_cast<char*>(1);*p=reinterpret_cast<char*>(1);*a=nullptr;*d=const_cast<char*>("SYNTHETIC_ALIAS");return TRUE;
}
static std::wstring title(HWND h) {wchar_t t[256]{};GetWindowTextW(h,t,256);return t;}
static BOOL CALLBACK findDialog(HWND h,LPARAM result) {
    if(title(h)==L"PL/SQL ActivityWatch - Settings") *reinterpret_cast<HWND*>(result)=h;return TRUE;
}
static BOOL CALLBACK findValidation(HWND h,LPARAM result) {
    if(title(h)==L"Settings validation") *reinterpret_cast<HWND*>(result)=h;return TRUE;
}
static HWND waitDialog() {
    for(int i=0;i<200;i++) {HWND h=nullptr;EnumThreadWindows(ownerThread,findDialog,reinterpret_cast<LPARAM>(&h));if(h) return h;std::this_thread::sleep_for(std::chrono::milliseconds(20));}return nullptr;
}
static void screenshot(HWND h) {
    RECT r{};GetWindowRect(h,&r);int width=r.right-r.left,height=r.bottom-r.top;
    HDC screen=GetDC(nullptr),memory=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,width,height);
    auto old=SelectObject(memory,bitmap);PrintWindow(h,memory,0);
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;
    info.bmiHeader.biHeight=-height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    std::string pixels(static_cast<size_t>(width)*height*4,0);SelectObject(memory,old);
    GetDIBits(memory,bitmap,0,height,pixels.data(),&info,DIB_RGB_COLORS);
    BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
    header.bfSize=header.bfOffBits+static_cast<DWORD>(pixels.size());
    std::ofstream out("settings-preview.bmp",std::ios::binary);out.write(reinterpret_cast<char*>(&header),sizeof(header));
    out.write(reinterpret_cast<char*>(&info.bmiHeader),sizeof(BITMAPINFOHEADER));out.write(pixels.data(),pixels.size());
    DeleteObject(bitmap);DeleteDC(memory);ReleaseDC(nullptr,screen);
}
static void pump(int ms) {
    auto end=GetTickCount64()+ms;
    while(GetTickCount64()<end) {MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)) {TranslateMessage(&m);DispatchMessageW(&m);}Sleep(10);}
}
static std::string history() {
    std::string text;
    auto path=std::filesystem::current_path()/L"synthetic-host-history"/L"plsql-activity-v1";
    if(!std::filesystem::exists(path)) return text;
    for(auto& e:std::filesystem::directory_iterator(path)) if(e.path().extension()==L".jsonl"&&!baseline.count(e.path())) {std::ifstream f(e.path());text.append(std::istreambuf_iterator<char>(f),{});}
    return text;
}
int main(int argc,char** argv) {
    try {
        ownerThread=GetCurrentThreadId();
        auto historyPath=std::filesystem::current_path()/L"synthetic-host-history"/L"plsql-activity-v1";
        if(std::filesystem::exists(historyPath)) for(auto& e:std::filesystem::directory_iterator(historyPath)) baseline.insert(e.path());
        for(const auto& filename:{L"ActivityTracker.dll",L"PLSQLActivity-test.dll"}) {
            auto library=LoadLibraryW(filename);check(library!=nullptr,"native DLL loads");
            for(auto name:{"BeforeExecuteWindow","IdentifyPlugIn","PlugInName","RegisterCallback","OnCreate","OnActivate","OnDeactivate","OnDestroy","OnWindowChange","OnWindowCreated","OnWindowClose","OnWindowConnectionChange","OnConnectionChange","CreateMenuItem","OnMenuClick","Configure","About"}) check(GetProcAddress(library,name)!=nullptr,"undecorated export exists");
            FreeLibrary(library);
        }
        if(argc>1&&std::string(argv[1])=="--exports-only") {
            std::cout<<"PASS headless native DLL loading and 17 SDK exports for production/test DLLs\n";
            return 0;
        }
        auto library=LoadLibraryW(L"PLSQLActivity-test.dll");
        auto invoke=[&](const char* name){reinterpret_cast<void(__cdecl*)()>(GetProcAddress(library,name))();};
        auto identify=reinterpret_cast<char*(__cdecl*)(int)>(GetProcAddress(library,"IdentifyPlugIn"));
        check(std::string(identify(123)).find("Local Activity")!=std::string::npos,"identification lifetime");
        invoke("OnCreate");check(callbackCalls==0,"OnCreate does not use callbacks");
        host=CreateWindowW(L"STATIC",L"Synthetic SDK host",WS_OVERLAPPEDWINDOW,0,0,500,400,nullptr,nullptr,nullptr,nullptr);
        child=CreateWindowW(L"STATIC",L"Synthetic worksheet - no SQL",WS_CHILD,0,0,50,50,host,nullptr,nullptr,nullptr);
        auto registerCallback=reinterpret_cast<void(__cdecl*)(int,void*)>(GetProcAddress(library,"RegisterCallback"));
        auto registerAll=[&] {
            registerCallback(1,reinterpret_cast<void*>(version));registerCallback(14,reinterpret_cast<void*>(type));
            registerCallback(16,reinterpret_cast<void*>(mainHandle));registerCallback(18,reinterpret_cast<void*>(childHandle));
            registerCallback(23,reinterpret_cast<void*>(file));registerCallback(110,reinterpret_cast<void*>(object));
            registerCallback(240,reinterpret_cast<void*>(connection));registerCallback(245,reinterpret_cast<void*>(connectionIndex));
        };
        registerAll();
        std::atomic<bool> automated{false},validated{false},reloaded{false};
        std::thread configure([&] {
            HWND d=waitDialog();if(!d) return;
            auto path=std::filesystem::current_path()/L"synthetic-host-history";
            SetDlgItemTextW(d,110,path.c_str());SetDlgItemInt(d,111,1,FALSE);SetDlgItemInt(d,112,60,FALSE);
            CheckDlgButton(d,120,BST_CHECKED);CheckDlgButton(d,121,BST_UNCHECKED);CheckDlgButton(d,124,BST_CHECKED);
            CheckDlgButton(d,128,BST_CHECKED);CheckDlgButton(d,129,BST_UNCHECKED);
            CheckDlgButton(d,127,argc>1?BST_CHECKED:BST_UNCHECKED);SetDlgItemInt(d,115,56661,FALSE);
            SetDlgItemInt(d,111,0,FALSE);PostMessageW(d,WM_COMMAND,IDOK,0);
            HWND validation=nullptr;
            for(int i=0;i<200&&!validation;i++) {EnumThreadWindows(ownerThread,findValidation,reinterpret_cast<LPARAM>(&validation));std::this_thread::sleep_for(std::chrono::milliseconds(10));}
            if(validation) {
                PostMessageW(validation,WM_COMMAND,MAKEWPARAM(IDOK,BN_CLICKED),reinterpret_cast<LPARAM>(GetDlgItem(validation,IDOK)));
                for(int i=0;i<200&&IsWindow(validation);i++) std::this_thread::sleep_for(std::chrono::milliseconds(10));
                validated=!IsWindow(validation);
            }
            SetDlgItemInt(d,111,1,FALSE);
            // Render the real resource after initialization, before saving.
            SendMessageW(d,WM_PAINT,0,0);screenshot(d);
            automated=true;PostMessageW(d,WM_COMMAND,IDOK,0);
        });
        invoke("Configure");configure.join();check(automated,"settings dialog controls initialized and saved");
        check(validated,"invalid heartbeat rejected by real settings UI");
        check(std::filesystem::exists(L"synthetic-settings/settings.ini"),"settings persistence");
        FreeLibrary(library);library=LoadLibraryW(L"PLSQLActivity-test.dll");
        check(library!=nullptr,"plugin reload for persisted settings test");
        registerCallback=reinterpret_cast<void(__cdecl*)(int,void*)>(GetProcAddress(library,"RegisterCallback"));
        registerAll();invoke("OnCreate");
        std::thread reload([&] {
            HWND d=waitDialog();if(!d) return;
            BOOL ok=FALSE;UINT heartbeat=GetDlgItemInt(d,111,&ok,FALSE);
            reloaded=ok&&heartbeat==1&&IsDlgButtonChecked(d,124)==BST_CHECKED&&
                IsDlgButtonChecked(d,127)==static_cast<UINT>(argc>1?BST_CHECKED:BST_UNCHECKED);
            PostMessageW(d,WM_COMMAND,IDCANCEL,0);
        });
        invoke("Configure");reload.join();check(reloaded,"saved settings reload and cancel works");
        ShowWindow(host,SW_SHOWNORMAL);SetForegroundWindow(host);pump(200);
        check(GetForegroundWindow()==host,"synthetic host could not obtain foreground focus; activity test cannot proceed");
        invoke("OnActivate");pump(2400);invoke("OnWindowChange");
        invoke("OnDeactivate");
        auto text=history();check(text.find("\"type\":\"heartbeat\"")!=std::string::npos,"mock host heartbeat captured");
        check(text.find("SYNTHETIC_OBJECT")!=std::string::npos,"database object captured");
        check(text.find("SYNTHETIC_ALIAS")!=std::string::npos,"optional TNS alias captured without dereferencing credentials");
        check(text.find("Synthetic worksheet")==std::string::npos,"window title is never stored");
        check(text.find("deactivated")!=std::string::npos,"disable closes worker interval");
        check(wrongThread==0,"all SDK callbacks stay on IDE thread");
        int prior=callbackCalls;pump(1200);check(callbackCalls==prior,"timer removed on deactivation");
        invoke("OnActivate");pump(1200);invoke("OnDestroy");
        DestroyWindow(host);FreeLibrary(library);
        check(std::filesystem::file_size("settings-preview.bmp")>1000,"UI preview captured");
        std::cout<<"PASS "<<assertions<<" SDK/export/UI assertions (synthetic host only)\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
