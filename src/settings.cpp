#include "settings.h"
#include <shlobj.h>
#include <fstream>
#include <stdexcept>
#include <wincred.h>

HINSTANCE moduleInstance=nullptr;
#ifdef ACTIVITY_TEST
static std::string testKey;
#endif
static std::string loadKey() {
#ifdef ACTIVITY_TEST
    return testKey;
#else
    PCREDENTIALW credential=nullptr;
    if(!CredReadW(L"PLSQLActivity/WakaTime",CRED_TYPE_GENERIC,0,&credential)) return "";
    std::string key(reinterpret_cast<char*>(credential->CredentialBlob),credential->CredentialBlobSize);CredFree(credential);return key;
#endif
}
static void storeKey(const std::string& key) {
#ifdef ACTIVITY_TEST
    testKey=key;
#else
    if(key.empty()) {CredDeleteW(L"PLSQLActivity/WakaTime",CRED_TYPE_GENERIC,0);return;}
    CREDENTIALW credential{};credential.Type=CRED_TYPE_GENERIC;credential.TargetName=const_cast<wchar_t*>(L"PLSQLActivity/WakaTime");
    credential.CredentialBlobSize=static_cast<DWORD>(key.size());credential.CredentialBlob=reinterpret_cast<LPBYTE>(const_cast<char*>(key.data()));
    credential.Persist=CRED_PERSIST_LOCAL_MACHINE;
    if(!CredWriteW(&credential,0)) throw std::runtime_error("Cannot save WakaTime key in Windows Credential Manager.");
#endif
}
std::wstring wide(const std::string& s) {
    int n=MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),nullptr,0);
    std::wstring w(n,0);MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),w.data(),n);return w;
}
std::string utf8(const std::wstring& w) {
    int n=WideCharToMultiByte(CP_UTF8,0,w.data(),static_cast<int>(w.size()),nullptr,0,nullptr,nullptr);
    std::string s(n,0);WideCharToMultiByte(CP_UTF8,0,w.data(),static_cast<int>(w.size()),s.data(),n,nullptr,nullptr);return s;
}
std::string sdkText(const char* value) {
    // SDK strings are borrowed, single-byte ANSI. Never retain host pointers.
    if(!value) return "";
    int n=MultiByteToWideChar(CP_ACP,0,value,-1,nullptr,0);
    std::wstring w(n,0);MultiByteToWideChar(CP_ACP,0,value,-1,w.data(),n);
    if(!w.empty()) w.pop_back();return utf8(w);
}
static std::filesystem::path configuration() {
#ifdef ACTIVITY_TEST
    auto result=std::filesystem::current_path()/L"synthetic-settings";
    std::filesystem::create_directories(result);return result/L"settings.ini";
#else
    wchar_t* p=nullptr;
    if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&p))) throw std::runtime_error("Cannot locate LocalAppData.");
    std::filesystem::path result=std::filesystem::path(p)/L"PLSQLActivity";CoTaskMemFree(p);
    std::filesystem::create_directories(result);return result/L"settings.ini";
#endif
}
static std::wstring read(const wchar_t* key,const wchar_t* fallback,const std::filesystem::path& path) {
    // Parse our own UTF-16 file: Win32 profile APIs can retain stale values across atomic replacement.
    if(!std::filesystem::exists(path)) return fallback;
    std::ifstream in(path,std::ios::binary);if(!in) throw std::runtime_error("Cannot read stored settings.");
    std::string bytes(std::istreambuf_iterator<char>(in),{});
    if(bytes.size()<2 || bytes.size()>131072 || bytes.size()%2 ||
        static_cast<unsigned char>(bytes[0])!=0xff || static_cast<unsigned char>(bytes[1])!=0xfe)
        throw std::runtime_error("Stored settings must be a valid UTF-16 settings file.");
    std::wstring text;
    for(size_t i=2;i<bytes.size();i+=2) text+=static_cast<wchar_t>(static_cast<unsigned char>(bytes[i])|(static_cast<unsigned char>(bytes[i+1])<<8));
    std::wstring prefix=std::wstring(key)+L"=";
    size_t offset=0;
    while(offset<text.size()) {
        auto end=text.find(L'\n',offset);auto line=text.substr(offset,end==std::wstring::npos?end:end-offset);
        if(!line.empty()&&line.back()==L'\r') line.pop_back();
        if(line.rfind(prefix,0)==0) return line.substr(prefix.size());
        if(end==std::wstring::npos) break;offset=end+1;
    }
    return fallback;
}
activity::Settings loadSettings() {
    activity::Settings s;auto path=configuration();s.directory=path.parent_path()/L"history";
    s.directory=read(L"directory",s.directory.c_str(),path);
    auto integer=[&](const wchar_t* k,int d) { auto text=read(k,std::to_wstring(d).c_str(),path);size_t n=0;int v=std::stoi(text,&n);if(n!=text.size()) throw std::runtime_error("Invalid stored numeric setting.");return v; };
    auto storedMode=read(L"payloadMode",L"__missing__",path);
    bool legacy=std::filesystem::exists(path)&&storedMode==L"__missing__";
    std::wstring mode=storedMode==L"__missing__"?(legacy?L"extended":L"strict"):storedMode.c_str();
    if(mode!=L"strict"&&mode!=L"extended") throw std::runtime_error("Invalid payload mode.");
    s.extended=mode==L"extended";
    for(int i=0;i<activity::ExtraFieldCount;i++) s.extraFields[i]=integer(wide("field_"+std::string(activity::extraFieldNames[i])).c_str(),legacy?1:static_cast<int>(s.extraFields[i]))!=0;
    s.enabled=integer(L"enabled",1)!=0;s.paused=integer(L"paused",0)!=0;
    s.files=integer(L"files",1)!=0;s.objects=integer(L"objects",1)!=0;s.connection=integer(L"connection",1)!=0;
    s.history=integer(L"history",0)!=0;s.csv=integer(L"csv",0)!=0;s.ics=integer(L"ics",0)!=0;s.aw=integer(L"aw",1)!=0;
    s.waka=integer(L"waka",0)!=0;s.wakaKey=loadKey();
    s.heartbeat=integer(L"heartbeat",15);s.idle=integer(L"idle",300);s.rotationMB=integer(L"rotationMB",8);
    s.retentionDays=integer(L"retentionDays",90);s.awPort=integer(L"awPort",5600);s.validate();return s;
}
activity::Settings defaultSettings() {
    activity::Settings s;s.directory=configuration().parent_path()/L"history";return s;
}
void saveSettings(const activity::Settings& s) {
    s.validate();auto path=configuration(),tmp=path;tmp+=L".tmp";
    // UTF-16 INI supports non-ASCII destination paths. Replace only after a complete write.
    std::wstring text=L"[activity]\r\nversion=2\r\ndirectory="+s.directory.wstring()+L"\r\n";
    auto add=[&](const wchar_t* k,int v){text+=std::wstring(k)+L"="+std::to_wstring(v)+L"\r\n";};
    add(L"enabled",s.enabled);add(L"paused",s.paused);add(L"files",s.files);add(L"objects",s.objects);add(L"connection",s.connection);
    add(L"csv",s.csv);add(L"ics",s.ics);add(L"aw",s.aw);add(L"heartbeat",s.heartbeat);add(L"idle",s.idle);
    add(L"rotationMB",s.rotationMB);add(L"retentionDays",s.retentionDays);add(L"awPort",s.awPort);
    add(L"history",s.history);add(L"waka",s.waka);
    text+=std::wstring(L"payloadMode=")+(s.extended?L"extended":L"strict")+L"\r\n";
    for(int i=0;i<activity::ExtraFieldCount;i++) add(wide("field_"+std::string(activity::extraFieldNames[i])).c_str(),s.extraFields[i]);
    if(s.wakaKey!=loadKey()) storeKey(s.wakaKey);
    std::ofstream o(tmp,std::ios::binary);const unsigned char bom[]={0xff,0xfe};o.write(reinterpret_cast<const char*>(bom),2);
    o.write(reinterpret_cast<const char*>(text.data()),text.size()*sizeof(wchar_t));o.close();
    if(!o || !MoveFileExW(tmp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) throw std::runtime_error("Cannot save settings.");
}
static std::wstring field(HWND d,int id) {
    int n=GetWindowTextLengthW(GetDlgItem(d,id));std::wstring text(n+1,0);
    GetDlgItemTextW(d,id,text.data(),n+1);text.resize(n);return text;
}
static void enableFields(HWND d) {
    BOOL history=IsDlgButtonChecked(d,128)==BST_CHECKED;
    for(int id:{110,113,114,125,126}) EnableWindow(GetDlgItem(d,id),history);
    EnableWindow(GetDlgItem(d,130),IsDlgButtonChecked(d,129)==BST_CHECKED);
    BOOL extended=IsDlgButtonChecked(d,131)==BST_CHECKED;
    for(int i=0;i<activity::ExtraFieldCount;i++) EnableWindow(GetDlgItem(d,140+i),extended);
    EnableWindow(GetDlgItem(d,123),extended);EnableWindow(GetDlgItem(d,124),extended);
}
static INT_PTR CALLBACK dialog(HWND d,UINT msg,WPARAM w,LPARAM l) {
    auto s=reinterpret_cast<activity::Settings*>(GetWindowLongPtrW(d,DWLP_USER));
    if(msg==WM_INITDIALOG) {
        s=reinterpret_cast<activity::Settings*>(l);SetWindowLongPtrW(d,DWLP_USER,l);
        SetDlgItemTextW(d,110,s->directory.c_str());
        int values[]={s->heartbeat,s->idle,s->rotationMB,s->retentionDays,s->awPort};
        for(int i=0;i<5;i++) SetDlgItemInt(d,111+i,values[i],FALSE);
        bool checks[]={s->enabled,s->paused,s->files,s->objects,s->connection,s->csv,s->ics,s->aw};
        for(int i=0;i<8;i++) CheckDlgButton(d,120+i,checks[i]?BST_CHECKED:BST_UNCHECKED);
        CheckDlgButton(d,128,s->history?BST_CHECKED:BST_UNCHECKED);CheckDlgButton(d,129,s->waka?BST_CHECKED:BST_UNCHECKED);
        CheckDlgButton(d,131,s->extended?BST_CHECKED:BST_UNCHECKED);
        for(int i=0;i<activity::ExtraFieldCount;i++) CheckDlgButton(d,140+i,s->extraFields[i]?BST_CHECKED:BST_UNCHECKED);
        SetDlgItemTextW(d,130,wide(s->wakaKey).c_str());enableFields(d);
        return TRUE;
    }
    if(msg==WM_COMMAND&&LOWORD(w)==IDCANCEL) { EndDialog(d,IDCANCEL);return TRUE; }
    if(msg==WM_COMMAND&&(LOWORD(w)==128||LOWORD(w)==129||LOWORD(w)==131)) {enableFields(d);return TRUE;}
    if(msg==WM_COMMAND&&LOWORD(w)==IDOK) {
        try {
            auto candidate=*s;candidate.directory=field(d,110);
            if(candidate.directory.wstring().find_first_of(L"\r\n")!=std::wstring::npos) throw std::runtime_error("Destination cannot contain line breaks.");
            int* values[]={&candidate.heartbeat,&candidate.idle,&candidate.rotationMB,&candidate.retentionDays,&candidate.awPort};
            for(int i=0;i<5;i++) { auto text=field(d,111+i);size_t n=0;*values[i]=std::stoi(text,&n);if(n!=text.size()) throw std::runtime_error("Enter whole numbers in all numeric fields."); }
            bool* checks[]={&candidate.enabled,&candidate.paused,&candidate.files,&candidate.objects,&candidate.connection,&candidate.csv,&candidate.ics,&candidate.aw};
            for(int i=0;i<8;i++) *checks[i]=IsDlgButtonChecked(d,120+i)==BST_CHECKED;
            candidate.history=IsDlgButtonChecked(d,128)==BST_CHECKED;candidate.waka=IsDlgButtonChecked(d,129)==BST_CHECKED;
            candidate.extended=IsDlgButtonChecked(d,131)==BST_CHECKED;
            for(int i=0;i<activity::ExtraFieldCount;i++) candidate.extraFields[i]=IsDlgButtonChecked(d,140+i)==BST_CHECKED;
            candidate.wakaKey=utf8(field(d,130));
            if(candidate.waka&&(candidate.wakaKey.empty()||candidate.wakaKey.size()>256||candidate.wakaKey.find_first_of("\r\n\t ")!=std::string::npos))
                throw std::runtime_error("Enter a valid WakaTime API key (without spaces).");
            candidate.validate();if(candidate.history) {std::filesystem::create_directories(candidate.directory);
            auto probe=candidate.directory/(".plsql-write-test-"+activity::newId());
            std::ofstream out(probe);out<<"write test";out.close();bool ok=static_cast<bool>(out);std::filesystem::remove(probe);
            if(!ok) throw std::runtime_error("Destination is not writable.");
            }
            saveSettings(candidate);*s=candidate;EndDialog(d,IDOK);
        } catch(const std::exception& e) { MessageBoxW(d,wide(e.what()).c_str(),L"Settings validation",MB_OK|MB_ICONERROR); }
        return TRUE;
    }
    return FALSE;
}
bool showSettings(HWND owner,activity::Settings& s) {
    INT_PTR result=DialogBoxParamW(moduleInstance,MAKEINTRESOURCEW(101),owner,dialog,reinterpret_cast<LPARAM>(&s));
    if(result==-1) throw std::runtime_error("Cannot open settings dialog.");return result==IDOK;
}
