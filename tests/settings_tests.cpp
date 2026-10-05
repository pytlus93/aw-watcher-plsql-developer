#include "../src/settings.h"
#include <iostream>
#include <stdexcept>
#include <fstream>

int main() {
    try {
        int checks=0;
        auto check=[&](bool value){++checks;if(!value) throw std::runtime_error("Settings round-trip failed");};
        auto first=defaultSettings();first.directory=std::filesystem::current_path()/L"synthetic \u010D settings";
        check(first.aw&&!first.history&&!first.waka);first.history=true;
        first.aw=false;first.awPort=5600;first.heartbeat=15;first.files=false;first.paused=true;
        saveSettings(first);auto initial=loadSettings();
        check(initial.directory==first.directory);check(initial.aw==false);check(initial.files==false);check(initial.paused==true);
        auto changed=first;changed.aw=true;changed.awPort=56661;changed.heartbeat=1;changed.connection=true;changed.paused=false;
        saveSettings(changed);auto loaded=loadSettings();
        check(loaded.aw==true);check(loaded.awPort==56661);check(loaded.heartbeat==1);check(loaded.connection==true);check(loaded.paused==false);
        loaded.retentionDays=0;bool invalid=false;try{saveSettings(loaded);}catch(...){invalid=true;}check(invalid);
        check(loadSettings().retentionDays==changed.retentionDays);
        changed.history=false;changed.waka=true;changed.wakaKey="synthetic-key";saveSettings(changed);auto destinations=loadSettings();
        check(!destinations.history&&destinations.waka&&destinations.wakaKey=="synthetic-key");
        std::ifstream ini(std::filesystem::current_path()/L"synthetic-settings"/L"settings.ini",std::ios::binary);
        std::string bytes(std::istreambuf_iterator<char>(ini),{}),utf16Key;for(char c:std::string("synthetic-key")){utf16Key+=c;utf16Key+='\0';}
        check(bytes.find(utf16Key)==std::string::npos);
        std::cout<<"PASS "<<checks<<" headless settings persistence assertions\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
