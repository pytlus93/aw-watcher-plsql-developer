#pragma once
#include "core.h"
#include <windows.h>
#include <winhttp.h>
#include <sstream>
#include <iomanip>
namespace activity {
inline std::string base64(const std::string& input) {
    const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;unsigned int value=0;int bits=-6;
    for(unsigned char c:input) {value=(value<<8)|c;bits+=8;while(bits>=0) {result+=alphabet[(value>>bits)&63];bits-=6;}}
    if(bits>-6) result+=alphabet[((value<<8)>>(bits+8))&63];
    while(result.size()%4) result+='=';return result;
}
inline std::string wakaPayload(int64_t time,const Context& c) {
    const bool file=!c.file.empty();
    std::string entity=file?c.file:"PL/SQL Developer";
    if(!file&&!c.objectName.empty()) entity+=": "+(c.owner.empty()?"":c.owner+".")+c.objectName;
    std::ostringstream o;o<<"{\"entity\":"<<quote(entity)<<",\"type\":"<<quote(file?"file":"app")
        <<",\"category\":\"coding\",\"time\":"<<std::fixed<<std::setprecision(3)<<time/1000.0
        <<",\"language\":\"PL/SQL\",\"is_write\":false}";return o.str();
}
class WakaTime {
    HINTERNET session=nullptr,connection=nullptr;
    std::wstring authorization;
    std::string lastEntity;
    int64_t lastSuccess=0,lastAttempt=0;
public:
    explicit WakaTime(const std::string& key,int testPort=0) {
        auto encoded=base64(key);authorization=L"Content-Type: application/json\r\nAuthorization: Basic "+std::wstring(encoded.begin(),encoded.end())+L"\r\n";
        session=WinHttpOpen(L"wakatime/1.0 windows PL/SQLDeveloper/16 plsql-activity/1.1",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
        if(session) {
            WinHttpSetTimeouts(session,500,500,1000,1000);
#ifdef ACTIVITY_TEST
            connection=WinHttpConnect(session,testPort?L"127.0.0.1":L"api.wakatime.com",static_cast<INTERNET_PORT>(testPort?testPort:443),0);
#else
            (void)testPort;connection=WinHttpConnect(session,L"api.wakatime.com",443,0);
#endif
        }
    }
    ~WakaTime() {if(connection) WinHttpCloseHandle(connection);if(session) WinHttpCloseHandle(session);}
    bool send(int64_t time,const Context& c,int testPort=0) {
        std::string entity=c.file.empty()?"PL/SQL Developer: "+c.owner+"."+c.objectName:c.file;
        if((lastSuccess&&time>=lastSuccess&&time-lastSuccess<120000&&entity==lastEntity) ||
            (lastAttempt&&time>=lastAttempt&&time-lastAttempt<15000)) return true;
        lastAttempt=time;if(!connection) return false;
        DWORD flags=WINHTTP_FLAG_SECURE;
#ifdef ACTIVITY_TEST
        if(testPort) flags=0;
#else
        (void)testPort;
#endif
        auto r=WinHttpOpenRequest(connection,L"POST",L"/api/v1/users/current/heartbeats",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,flags);
        if(!r) return false;
        DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;WinHttpSetOption(r,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect));
        auto body=wakaPayload(time,c);
        BOOL ok=WinHttpSendRequest(r,authorization.c_str(),static_cast<DWORD>(-1),body.data(),static_cast<DWORD>(body.size()),static_cast<DWORD>(body.size()),0);
        if(ok) ok=WinHttpReceiveResponse(r,nullptr);
        DWORD status=0,n=sizeof(status);if(ok) ok=WinHttpQueryHeaders(r,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&n,WINHTTP_NO_HEADER_INDEX);
        WinHttpCloseHandle(r);
        if(ok&&status>=200&&status<300) {lastSuccess=time;lastEntity=entity;return true;}return false;
    }
};
}
