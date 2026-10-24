#include "settings.h"
#include "aw.h"
#include "wakatime.h"
#include "sql_context.h"
#include <winhttp.h>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>

using namespace activity;
namespace {
void* callbacks[254]{};
DWORD ideThread=0;
HWND timerWindow=nullptr, mainWindow=nullptr;
std::map<HWND,std::string> windows;
std::map<HWND,std::string> references;
Settings settings;
bool initialized=false, activated=false;
std::atomic<bool> failed{false};
std::mutex lock;
std::condition_variable ready;
std::deque<Sample> pending;
std::thread writer;
bool stopping=false;
int64_t stopTime=0;
std::string stopReason;
template<typename T> T cb(int index) { return reinterpret_cast<T>(callbacks[index]); }
using GetInt=int(__cdecl*)();
using GetHandle=HWND(__cdecl*)();
using GetText=char*(__cdecl*)();
using GetObject=BOOL(__cdecl*)(char**,char**,char**,char**);
using GetConnection=BOOL(__cdecl*)(int,char**,char**,char**,char**);
int64_t now() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
void error(const wchar_t* message) { MessageBoxW(mainWindow,message,L"ActivityTracker",MB_OK|MB_ICONERROR); }

// Only the storage worker owns WinHTTP handles. It never uses host callbacks.
void startWorker() {
    if(writer.joinable()) return;
    { std::lock_guard<std::mutex> guard(lock);pending.clear();stopping=false; }
    failed=false;auto config=settings;
    writer=std::thread([config] {
        try {
            Engine engine(config);AW aw(config.effectiveAwPort(),config.extended&&config.extraFields[DeliverySegmentId]);
            std::unique_ptr<WakaTime> waka;
            if(config.waka&&!config.wakaKey.empty()) waka=std::make_unique<WakaTime>(config.wakaKey);
            if(waka) engine.contextDelivery=[&](int64_t time,const Context& context){waka->send(time,context);};
            if(config.aw) engine.heartbeatDelivery=[&](const std::string& body){aw.send(body,config.heartbeat+2,engine.deliveryBoundary);};
            for(;;) {
                Sample s;
                { std::unique_lock<std::mutex> guard(lock);ready.wait(guard,[]{return stopping||!pending.empty();});
                    if(pending.empty()&&stopping) break;
                    s=std::move(pending.front());pending.pop_front(); }
                engine.sample(s);
            }
            engine.stop(stopTime,stopReason);
        } catch(...) { failed=true; }
    });
}
void stopWorker(const std::string& reason) {
    if(!writer.joinable()) return;
    { std::lock_guard<std::mutex> guard(lock);stopping=true;stopTime=now();stopReason=reason; }
    ready.notify_one();writer.join();
}
void collectSample() {
    if(!activated || GetCurrentThreadId()!=ideThread || failed) return;
    Sample s;s.utcMs=now();s.monotonicMs=GetTickCount64();
    DWORD process=0;GetWindowThreadProcessId(GetForegroundWindow(),&process);
    s.focused=process==GetCurrentProcessId();
    LASTINPUTINFO input{sizeof(input)};
    s.idle=!GetLastInputInfo(&input) || static_cast<DWORD>(GetTickCount()-input.dwTime)>=static_cast<DWORD>(settings.idle)*1000;
    // Avoid reading metadata while backgrounded/paused, including borrowed credential pointers.
    if(s.focused&&!s.idle&&settings.enabled&&!settings.paused) {
        auto type=cb<GetInt>(14);auto child=cb<GetHandle>(18);
        HWND h=child?child():nullptr;s.context.windowType=type?type():0;
        if(h&&s.context.windowType>=1&&s.context.windowType<=6) {
            if(!windows.count(h)) windows[h]=newId();s.context.window=windows[h];
            if(settings.files) if(auto filename=cb<GetText>(23)) s.context.file=sdkText(filename());
            if(settings.extended&&settings.objects&&(settings.extraFields[ObjectType]||settings.extraFields[ObjectOwner]||settings.extraFields[ObjectName])) if(auto object=cb<GetObject>(110)) {
                char *t=nullptr,*o=nullptr,*n=nullptr,*sub=nullptr;
                if(object(&t,&o,&n,&sub)) {s.context.objectType=sdkText(t);s.context.owner=sdkText(o);s.context.objectName=sdkText(n);}
            }
            if(settings.extended&&settings.objects&&settings.extraFields[ObjectType]&&settings.extraFields[ObjectName]&&references.count(h)&&!references[h].empty()&&s.context.objectName.empty()) {
                s.context.objectType="SQL_REFERENCE";s.context.objectName=references[h];
            }
            if(settings.extended&&settings.connection&&settings.extraFields[ConnectionAlias]) {
                auto index=cb<GetInt>(245);auto info=cb<GetConnection>(240);
                int ix=index?index():-1;
                if(ix>=0&&info) {
                    char *username=nullptr,*password=nullptr,*database=nullptr,*connectAs=nullptr;
                    if(info(ix,&username,&password,&database,&connectAs)) s.context.connection=sdkText(database);
                    // Host API requires these output parameters; do not dereference, copy or log username/password.
                }
            }
        }
    }
    for(auto it=windows.begin();it!=windows.end();) if(!IsWindow(it->first)) {references.erase(it->first);it=windows.erase(it);}else ++it;
    { std::lock_guard<std::mutex> guard(lock);
        if(pending.size()>=256) {pending.clear();s.discontinuity=true;}
        pending.push_back(std::move(s)); }
    ready.notify_one();
}
void sample() noexcept {
    try {collectSample();} catch(...) {failed=true;}
}
LRESULT CALLBACK procedure(HWND h,UINT msg,WPARAM w,LPARAM l) {
    if(msg==WM_TIMER) {
        if(failed) {KillTimer(h,1);error(L"Tracking stopped after an internal or storage error. Open Settings to retry.");}
        else sample();
        return 0;
    }
    return DefWindowProcW(h,msg,w,l);
}
void configure() {
    if(ideThread&&GetCurrentThreadId()!=ideThread) return;
    try {
        if(!initialized) {
            try {settings=loadSettings();}
            catch(const std::exception&) {
                settings=defaultSettings();settings.paused=true;
                error(L"Stored settings are invalid. Defaults are loaded with recording paused. Correct the values and Save to replace the invalid settings.");
            }
            initialized=true;
        }
        stopWorker("settings");
        showSettings(mainWindow,settings);
        if(activated) {startWorker();SetTimer(timerWindow,1,1000,nullptr);sample();}
    } catch(const std::exception& e) {error(wide(e.what()).c_str());}
}
}
extern "C" {
char* __cdecl PlugInName() { return const_cast<char*>("ActivityTracker"); }
char* __cdecl IdentifyPlugIn(int) { return const_cast<char*>("ActivityTracker"); }
void __cdecl RegisterCallback(int index,void* address) { if(index>=0&&index<254) callbacks[index]=address; }
void __cdecl OnCreate() { /* Host callbacks are deliberately unused until OnActivate. */ }
void __cdecl OnActivate() {
    try {
        if(activated) return;ideThread=GetCurrentThreadId();
        auto main=cb<GetHandle>(16);mainWindow=main?main():nullptr;
        if(!cb<GetInt>(14)||!cb<GetHandle>(18)||!mainWindow) throw std::runtime_error("Required host callbacks unavailable; recording disabled.");
        if(!initialized) {settings=loadSettings();initialized=true;}
        WNDCLASSW wc{};wc.lpfnWndProc=procedure;wc.hInstance=moduleInstance;wc.lpszClassName=L"PLSQLActivityTimerV1";
        RegisterClassW(&wc);
        timerWindow=CreateWindowW(wc.lpszClassName,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,moduleInstance,nullptr);
        if(!timerWindow) throw std::runtime_error("Cannot create IDE-thread timer.");
        activated=true;startWorker();
        if(!SetTimer(timerWindow,1,1000,nullptr)) throw std::runtime_error("Cannot start activity timer.");sample();
    } catch(const std::exception& e) {
        activated=false;
        if(timerWindow) {KillTimer(timerWindow,1);DestroyWindow(timerWindow);timerWindow=nullptr;}
        stopWorker("activation_failed");error(wide(e.what()).c_str());
    }
}
void __cdecl OnDeactivate() {
    activated=false;if(timerWindow) {KillTimer(timerWindow,1);DestroyWindow(timerWindow);timerWindow=nullptr;}
    stopWorker("deactivated");windows.clear();references.clear();
}
void __cdecl OnDestroy() {OnDeactivate();UnregisterClassW(L"PLSQLActivityTimerV1",moduleInstance);}
void __cdecl OnWindowChange() {sample();}
void __cdecl OnConnectionChange() {sample();}
void __cdecl OnWindowConnectionChange() {sample();}
void __cdecl OnWindowCreated(int) {
    if(GetCurrentThreadId()!=ideThread) return;
    if(auto child=cb<GetHandle>(18)) {windows.erase(child());references.erase(child());}sample();
}
BOOL __cdecl BeforeExecuteWindow(int windowType) {
    try {
        if(GetCurrentThreadId()!=ideThread||!settings.extended||!settings.objects||!settings.extraFields[ObjectType]||!settings.extraFields[ObjectName]||!settings.enabled||settings.paused||windowType!=1) return TRUE;
        auto child=cb<GetHandle>(18);HWND h=child?child():nullptr;if(!h)return TRUE;
        const char* source=nullptr;if(auto selected=cb<GetText>(31)) source=selected();
        if(!source||!*source) if(auto text=cb<GetText>(30))source=text();
        std::string metadata;
        if(source) {
            size_t length=strnlen_s(source,65537);
            if(length<=65536) {std::string bounded(source,length);auto sql=sdkText(bounded.c_str());metadata=sqlReferences(sql);
                if(!sql.empty())SecureZeroMemory(sql.data(),sql.size());if(!bounded.empty())SecureZeroMemory(bounded.data(),bounded.size());}
        }
        references[h]=metadata;sample();
    } catch(...) { /* Context inference must never block SQL execution. */ }
    return TRUE;
}
int __cdecl OnWindowClose(int,BOOL) {
    if(GetCurrentThreadId()!=ideThread) return 0;
    // Close may subsequently be cancelled by the host. Keep the window identity until destruction.
    Sample s;s.utcMs=now();s.monotonicMs=GetTickCount64();
    {std::lock_guard<std::mutex> guard(lock);pending.push_back(s);}ready.notify_one();return 0;
}
char* __cdecl CreateMenuItem(int index) {
    auto version=cb<GetInt>(1);bool ribbon=version&&version()>=1200;
    // Without TAB=, the SDK places the group in the existing Plug-Ins ribbon.
    if(index==1) return const_cast<char*>(ribbon?"GROUP=ActivityTracker":"Tools / ActivityTracker / -");
    if(index==2) return const_cast<char*>(ribbon?"ITEM=Settings":"Tools / ActivityTracker / Settings");
    return const_cast<char*>("");
}
void __cdecl OnMenuClick(int index) {if(index==2) configure();}
void __cdecl Configure() {configure();}
char* __cdecl About() {return const_cast<char*>("ActivityTracker 1.4.0\nhttps://github.com/pytlus93/aw-watcher-plsql-developer\nActivityWatch and optional WakaTime delivery. Local activity files are optional and default off.\nSQL contents and database credentials are never sent or logged.");}
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID) {
    if(reason==DLL_PROCESS_ATTACH) moduleInstance=instance;return TRUE;
}
