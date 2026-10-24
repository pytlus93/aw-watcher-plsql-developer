#include "../src/settings.h"
#include <iostream>
#include <stdexcept>
#include <fstream>

int main() {
    try {
        int checks=0;
        auto check=[&](bool value){++checks;if(!value) throw std::runtime_error("Settings round-trip failed");};
        auto first=defaultSettings();first.directory=std::filesystem::current_path()/L"synthetic \u010D settings";
        check(!first.testing&&first.effectiveAwPort()==5600);
        auto testing=first;testing.testing=true;check(testing.effectiveAwPort()==5666);
        testing.awPort=5678;check(testing.effectiveAwPort()==5678);
        saveSettings(testing);auto testingLoaded=loadSettings();
        check(testingLoaded.testing&&testingLoaded.awPort==5678&&testingLoaded.effectiveAwPort()==5678);
        testing.awPort=5600;saveSettings(testing);check(loadSettings().effectiveAwPort()==5666);
        check(first.aw&&!first.history&&!first.waka&&!first.extended&&!first.extraFields[activity::ObjectName]&&!first.extraFields[activity::ConnectionAlias]);first.history=true;
        first.aw=false;first.awPort=5600;first.heartbeat=15;first.files=false;first.paused=true;
        saveSettings(first);auto initial=loadSettings();
        check(initial.directory==first.directory);check(initial.aw==false);check(initial.files==false);check(initial.paused==true);
        auto changed=first;changed.aw=true;changed.awPort=56661;changed.heartbeat=1;changed.connection=true;changed.paused=false;
        saveSettings(changed);auto loaded=loadSettings();
        check(loaded.aw==true);check(loaded.awPort==56661);check(loaded.heartbeat==1);check(loaded.connection==true);check(loaded.paused==false);
        loaded.retentionDays=0;bool invalid=false;try{saveSettings(loaded);}catch(...){invalid=true;}check(invalid);
        check(loadSettings().retentionDays==changed.retentionDays);
        changed.extended=true;changed.extraFields[activity::ObjectOwner]=false;
        changed.history=false;changed.waka=true;changed.wakaKey="synthetic-key";saveSettings(changed);auto destinations=loadSettings();
        check(destinations.extended&&!destinations.extraFields[activity::ObjectOwner]);
        check(!destinations.history&&destinations.waka&&destinations.wakaKey=="synthetic-key");
        std::ifstream ini(std::filesystem::current_path()/L"synthetic-settings"/L"settings.ini",std::ios::binary);
        std::string bytes(std::istreambuf_iterator<char>(ini),{}),utf16Key;for(char c:std::string("synthetic-key")){utf16Key+=c;utf16Key+='\0';}
        ini.close();
        check(bytes.find(utf16Key)==std::string::npos);
        auto path=std::filesystem::current_path()/L"synthetic-settings"/L"settings.ini";
        auto replaceIni=[&](const std::wstring& text){std::ofstream out(path,std::ios::binary);const unsigned char bom[]={0xff,0xfe};out.write(reinterpret_cast<const char*>(bom),2);out.write(reinterpret_cast<const char*>(text.data()),text.size()*sizeof(wchar_t));out.close();};
        std::wstring legacy=L"[activity]\r\nversion=1\r\nfiles=0\r\nobjects=0\r\nconnection=0\r\n";
        replaceIni(legacy);auto migrated=loadSettings();
        check(!migrated.testing&&migrated.effectiveAwPort()==5600);
        check(migrated.extended&&!migrated.files&&!migrated.objects&&!migrated.connection);
        migrated.extraFields[activity::WindowId]=false;saveSettings(migrated);
        check(loadSettings().extended&&!loadSettings().extraFields[activity::WindowId]);
        replaceIni(L"[activity]\r\npayloadMode=invalid\r\n");invalid=false;try{loadSettings();}catch(...){invalid=true;}check(invalid);
        saveSettings(changed);
        std::cout<<"PASS "<<checks<<" headless settings persistence assertions\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
