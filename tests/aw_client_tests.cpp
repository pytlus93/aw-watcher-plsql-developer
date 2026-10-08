#include "../src/settings.h"
#include "../src/aw.h"
#include <iostream>
#include <chrono>

int main(int argc,char**) {
    try {
        auto s=defaultSettings();s.directory=std::filesystem::current_path()/L"synthetic-headless-aw";
        s.aw=true;s.awPort=56661;s.heartbeat=1;s.connection=true;s.csv=false;s.ics=false;
        saveSettings(s);auto loaded=loadSettings();
        if(!loaded.aw||loaded.awPort!=56661) throw std::runtime_error("AW settings did not persist");
        activity::AW aw(loaded.awPort);activity::Engine engine(loaded);
        engine.heartbeatDelivery=[&](const std::string& body){aw.send(body,3);};
        activity::Context c;c.window="synthetic-window";c.windowType=1;c.connection="SYNTHETIC_ALIAS";
        auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        const int samples=argc>1?32:4;
        for(int i=0;i<samples;i++) {engine.sample({now+i*1000,i*1000,true,false,c});if(argc>1)Sleep(250);}
        engine.stop(now+samples*1000,"test_shutdown");
        std::cout<<"PASS headless core-to-WinHTTP delivery with reloaded settings\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
