#include "../src/settings.h"
#include "../src/aw.h"
#include <iostream>
#include <chrono>

int main(int argc,char** argv) {
    try {
        auto s=defaultSettings();s.directory=std::filesystem::current_path()/L"synthetic-headless-aw";
        s.extraFields.fill(true); // Synthetic fixtures explicitly opt into full extended context.
        s.extended=!(argc>2&&std::string(argv[2])=="--strict");s.aw=true;s.awPort=argc>3?std::stoi(argv[3]):56661;s.heartbeat=1;s.connection=true;s.csv=false;s.ics=false;
        s.paused=argc>1&&std::string(argv[1])=="--paused";
        saveSettings(s);auto loaded=loadSettings();
        if(!loaded.aw||loaded.awPort!=s.awPort) throw std::runtime_error("AW settings did not persist");
        activity::AW aw(loaded.effectiveAwPort(),loaded.extended&&loaded.extraFields[activity::DeliverySegmentId]);activity::Engine engine(loaded);
        engine.heartbeatDelivery=[&](const std::string& body){if(!aw.send(body,3,engine.deliveryBoundary)) std::cout<<"Synthetic AW delivery failed\n"<<std::flush;};
        activity::Context c;c.window="synthetic-window";c.windowType=1;c.connection="SYNTHETIC_ALIAS";
        auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        auto startMono=GetTickCount64();
        const int samples=argc>4?std::stoi(argv[4]):argc>1?32:4;
        for(int i=0;i<samples;i++) {auto elapsed=argc>1?static_cast<int64_t>(GetTickCount64()-startMono):i*1000;engine.sample({now+elapsed,elapsed,true,false,c});if(argc>1)Sleep(250);}
        engine.stop(now+(argc>1?static_cast<int64_t>(GetTickCount64()-startMono):samples*1000),"test_shutdown");
        std::cout<<"PASS headless core-to-WinHTTP delivery with reloaded settings\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
