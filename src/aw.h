#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#include "settings.h"
namespace activity {
class AW {
    HINTERNET session=nullptr, connection=nullptr;
    bool bucketReady=false;
    int port;
    std::wstring bucket;
    std::string hostname;
    bool request(const wchar_t* verb,const std::wstring& path,const std::string& body) {
        if(!connection) return false;
        HINTERNET r=WinHttpOpenRequest(connection,verb,path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,0);
        if(!r) return false;
        BOOL ok=WinHttpSendRequest(r,L"Content-Type: application/json\r\n",static_cast<DWORD>(-1),
            const_cast<char*>(body.data()),static_cast<DWORD>(body.size()),static_cast<DWORD>(body.size()),0);
        if(ok) ok=WinHttpReceiveResponse(r,nullptr);
        DWORD status=0,n=sizeof(status);
        if(ok) ok=WinHttpQueryHeaders(r,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&n,WINHTTP_NO_HEADER_INDEX);
        WinHttpCloseHandle(r);return ok&&status>=200&&status<300;
    }
public:
    explicit AW(int p):port(p) {
        wchar_t name[MAX_COMPUTERNAME_LENGTH+1]{};DWORD length=MAX_COMPUTERNAME_LENGTH+1;
        if(!GetComputerNameW(name,&length)) throw std::runtime_error("Cannot determine Windows computer name.");
        hostname=utf8(std::wstring(name,length));bucket=L"aw-watcher-plsql_";
        // Percent-encode the UTF-8 name for the REST path while preserving bucket identity.
        const char* hex="0123456789ABCDEF";
        for(unsigned char c:hostname) {
            if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_') bucket+=static_cast<wchar_t>(c);
            else {bucket+=L'%';bucket+=hex[c>>4];bucket+=hex[c&15];}
        }
        session=WinHttpOpen(L"PLSQLActivity/1",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
        if(session) { WinHttpSetTimeouts(session,300,300,500,500);connection=WinHttpConnect(session,L"127.0.0.1",static_cast<INTERNET_PORT>(port),0); }
    }
    ~AW() { if(connection) WinHttpCloseHandle(connection);if(session) WinHttpCloseHandle(session); }
    void send(const std::string& payload,int pulse) {
        if(!bucketReady) bucketReady=request(L"POST",L"/api/0/buckets/"+bucket,
            "{\"client\":\"aw-watcher-plsql\",\"type\":\"app.editor.activity\",\"hostname\":"+quote(hostname)+"}");
        if(bucketReady && !request(L"POST",L"/api/0/buckets/"+bucket+L"/heartbeat?pulsetime="+std::to_wstring(pulse),payload)) bucketReady=false;
        // Delivery failures never enable file logging implicitly; retry on the next live pulse.
    }
};
}
