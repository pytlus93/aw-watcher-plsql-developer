#include "../src/core.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <stdexcept>
#include <sstream>
#include "../src/sql_context.h"

using namespace activity;
static int assertions=0;
static void check(bool value,const char* message) {++assertions;if(!value) throw std::runtime_error(message);}
static std::string read(const std::filesystem::path& p) {std::ifstream f(p,std::ios::binary);return std::string(std::istreambuf_iterator<char>(f),{});}
static std::vector<std::string> records(const std::filesystem::path& root) {
    std::vector<std::string> result;
    for(const auto& e:std::filesystem::directory_iterator(root/"plsql-activity-v1")) if(e.path().extension()==".jsonl") {
        std::ifstream f(e.path());std::string line;while(std::getline(f,line)) result.push_back(line);
    }
    return result;
}
static int count(const std::vector<std::string>& rows,const std::string& text) {
    int n=0;for(auto& r:rows) if(r.find(text)!=std::string::npos)++n;return n;
}
int main() {
    try {
        const int64_t base=1791187200000;
        auto root=std::filesystem::current_path()/"synthetic-core"/newId();
        Settings s;s.history=true;s.csv=true;s.ics=true;s.connection=false;s.directory=root;s.heartbeat=2;s.idle=10;
        Context c;c.window="worksheet-1";c.windowType=1;c.file="=synthetic,\"file\"\n.sql";c.objectName="TEST_OBJECT";c.owner="TEST_OWNER";c.connection="SYNTHETIC_TNS";
        std::vector<std::string> delivery;
        Engine e(s,"synthetic-session");e.heartbeatDelivery=[&](const std::string& p){delivery.push_back(p);};
        e.sample({base,1000,true,false,c});e.sample({base+1000,2000,true,false,c});e.sample({base+2000,3000,true,false,c});
        e.sample({base+3000,4000,false,false,c});
        auto rows=records(root);
        check(count(rows,"\"type\":\"start\"")==1,"start once");
        check(count(rows,"\"type\":\"heartbeat\"")==2,"periodic heartbeat");
        check(count(rows,"\"duration_ms\":3000")==1,"focus loss boundary");
        check(delivery.size()==3,"AW periodic and closing heartbeats");
        check(delivery[0].find("interval_id")!=std::string::npos,"prevent AW merging across focus gaps");
        for(auto& r:rows) {check(r.find("schema_version")!=std::string::npos,"versioned");check(r.find("SYNTHETIC_TNS")==std::string::npos,"connection privacy default");}
        int csv=0,ics=0;
        for(auto& f:std::filesystem::directory_iterator(root/"plsql-activity-v1")) {
            if(f.path().extension()==".csv") {++csv;check(read(f.path()).find("'=synthetic")!=std::string::npos,"CSV formula safe");check(read(f.path()).find("\"\"file\"\"")!=std::string::npos,"CSV quotes escaped");}
            if(f.path().extension()==".ics") {++ics;auto text=read(f.path());check(text.find("BEGIN:VCALENDAR\r\n")!=std::string::npos,"valid calendar wrapper");check(text.find("\\n.sql")!=std::string::npos,"ICS newline escaped");check(text.find("DTEND:")!=std::string::npos,"completed ICS end");}
        }
        check(csv==1&&ics==1,"only completed intervals exported");
        e.sample({base+4000,5000,true,false,c});e.sample({base+5000,6000,true,true,c});
        rows=records(root);check(count(rows,"\"reason\":\"idle\"")==1,"idle closes");
        e.sample({base+6000,7000,true,false,c});c.window="worksheet-2";e.sample({base+7000,8000,true,false,c});
        check(count(records(root),"\"reason\":\"context_changed\"")==1,"unsaved worksheet distinction");
        e.sample({base+100000,101000,true,false,c});
        rows=records(root);check(count(rows,"\"reason\":\"gap\"")==1,"suspend gap closes at last observation");
        check(count(rows,"\"duration_ms\":93000")==0,"no fabricated gap activity");
        e.stop(base+110000,"shutdown");rows=records(root);check(count(rows,"\"reason\":\"shutdown\"")==1,"graceful shutdown");
        Settings privateSettings=s;privateSettings.directory=root/"private";privateSettings.files=false;privateSettings.objects=false;
        Engine privateEngine(privateSettings);privateEngine.sample({base,1,true,false,c});privateEngine.stop(base+1000,"shutdown");
        for(auto& r:records(privateSettings.directory)) {check(r.find("synthetic,")==std::string::npos,"file privacy");check(r.find("TEST_OWNER")==std::string::npos,"object privacy");}
        Settings disabled=s;disabled.directory=root/"disabled";disabled.paused=true;
        Engine paused(disabled);paused.sample({base,1,true,false,c});check(!std::filesystem::exists(disabled.directory),"pause creates no history");
        Settings invalid=s;invalid.heartbeat=0;bool threw=false;try{invalid.validate();}catch(...){threw=true;}check(threw,"settings validation");
        Journal journal(s,"rotation");std::string large(600000,'x');journal.append(base,large);journal.append(base,large);
        int rotated=0;for(auto& f:std::filesystem::directory_iterator(root/"plsql-activity-v1")) if(f.path().filename().string().find("rotation")!=std::string::npos)++rotated;
        // Default is 8 MB; exercise configured 1 MB separately.
        check(rotated==1,"rotation respects configured size");
        s.rotationMB=1;Journal small(s,"small");small.append(base,large);small.append(base,large);rotated=0;
        for(auto& f:std::filesystem::directory_iterator(root/"plsql-activity-v1")) if(f.path().filename().string().find("small")!=std::string::npos)++rotated;
        check(rotated==2,"size rotation");
        small.append(base+86400000,"{\"synthetic\":true}");rotated=0;
        for(auto& f:std::filesystem::directory_iterator(root/"plsql-activity-v1")) if(f.path().filename().string().find("small")!=std::string::npos)++rotated;
        check(rotated==3,"UTC day rotation");
        Context longContext=c;longContext.file.clear();for(int i=0;i<100;i++) longContext.file+=u8"\u010D";
        small.exportInterval("synthetic-unicode",base,base+1000,longContext);
        for(auto& f:std::filesystem::directory_iterator(root/"plsql-activity-v1")) if(f.path().filename().string().find("synthetic-unicode.ics")!=std::string::npos) {
            std::istringstream lines(read(f.path()));std::string line;while(std::getline(lines,line)) check(line.size()<=76,"ICS byte line folding");
        }
        auto namespaceRoot=root/"plsql-activity-v1";
        std::ofstream(namespaceRoot/"keep.txt")<<"synthetic";
        std::ofstream(namespaceRoot/"activity-2000-01-01-synthetic-0.jsonl")<<"synthetic";
        std::ofstream(root/"activity-2000-01-01-outside.jsonl")<<"synthetic";
        journal.expire(base);check(!std::filesystem::exists(namespaceRoot/"activity-2000-01-01-synthetic-0.jsonl"),"retention expires own files");
        check(std::filesystem::exists(namespaceRoot/"keep.txt"),"retention keeps unrelated files");check(std::filesystem::exists(root/"activity-2000-01-01-outside.jsonl"),"retention isolated");
        Settings unwritable=s;unwritable.directory=root/"blocked";std::ofstream(unwritable.directory)<<"synthetic";
        Engine bad(unwritable);threw=false;try{bad.sample({base,1,true,false,c});}catch(...){threw=true;}check(threw,"storage failure surfaced");
        Settings clocks=s;clocks.directory=root/"clocks";Engine clockEngine(clocks);
        clockEngine.sample({base,1000,true,false,c});clockEngine.sample({base-1000,2000,true,false,c});
        check(count(records(clocks.directory),"\"type\":\"gap\"")==1,"clock rollback splits");
        Settings stalled=s;stalled.directory=root/"stalled";stalled.heartbeat=300;stalled.idle=300;
        Engine stalledEngine(stalled);stalledEngine.sample({base,1000,true,false,c});stalledEngine.sample({base+4000,5000,true,false,c});
        check(count(records(stalled.directory),"\"type\":\"gap\"")==1,"sampling gap independent of heartbeat setting");
        Sample lost{base+5000,6000,true,false,c};lost.discontinuity=true;stalledEngine.sample(lost);
        check(count(records(stalled.directory),"\"type\":\"gap\"")==2,"queue overflow explicitly splits");
        check(quote("\t\"\\")=="\"\\u0009\\\"\\\\\"","JSON escaping");
        check(utc(base).find("Z")!=std::string::npos,"UTC timestamp");
        auto titlePulse=[&](Settings config,Context value) {
            config.history=false;std::string result;Engine engine(config);
            engine.heartbeatDelivery=[&](const std::string& body){result=body;};
            engine.sample({base,1000,true,false,value});engine.stop(base+1000,"test");return result;
        };
        Context titled;titled.window="title-test";titled.file="C:\\synthetic\\demo.sql";
        titled.owner="SYNTHETIC";titled.objectName="ORDERS";titled.connection="TEST_ALIAS";
        Settings titles;titles.connection=true;
        check(titlePulse(titles,titled).find("\"title\":\"demo.sql - SYNTHETIC.ORDERS @ TEST_ALIAS\"")!=std::string::npos,"readable title from enabled metadata");
        titles.files=false;titles.objects=false;titles.connection=false;
        auto privatePulse=titlePulse(titles,titled);
        check(privatePulse.find("\"title\":\"Activity (context unavailable)\"")!=std::string::npos,"neutral title after privacy redaction");
        check(privatePulse.find("demo.sql")==std::string::npos&&privatePulse.find("ORDERS")==std::string::npos&&privatePulse.find("TEST_ALIAS")==std::string::npos,"disabled metadata absent from delivered title and fields");
        titles.objects=true;titled.objectType="SQL_REFERENCE";
        check(titlePulse(titles,titled).find("References (inferred): ORDERS")!=std::string::npos,"title labels inferred identifiers");
        titled.objectType.clear();titled.owner.clear();titled.objectName=std::string(1024,'X');
        check(titlePulse(titles,titled).find("\"title\":\""+std::string(237,'X')+"...\"")!=std::string::npos,"long metadata produces a concise title");
        titled.objectName.clear();for(int i=0;i<125;i++)titled.objectName+=u8"\u011B";
        std::string clipped;for(int i=0;i<118;i++)clipped+=u8"\u011B";
        check(titlePulse(titles,titled).find("\"title\":\""+clipped+"...\"")!=std::string::npos,"title truncation preserves UTF-8");
        Settings noFiles;noFiles.directory=root/"must-not-exist";
        noFiles.csv=true;noFiles.ics=true;int pulses=0;
        Engine networkOnly(noFiles);networkOnly.heartbeatDelivery=[&](const std::string&){++pulses;};
        networkOnly.sample({base,1000,true,false,c});networkOnly.sample({base+1000,2000,false,false,c});networkOnly.stop(base+2000,"shutdown");
        check(pulses==2,"AW start and closing heartbeat with all activity files off");
        check(!std::filesystem::exists(noFiles.directory),"no directories, JSONL, CSV or ICS with local activity files off");
        check(sqlReferences("select 'FROM SECRET' from synthetic.orders o join synthetic.v_orders v on 1=1 -- join secret\n") == "SYNTHETIC.ORDERS, SYNTHETIC.V_ORDERS","SQL tables/views extracted without literals or comments");
        check(sqlReferences("with tmp as (select * from synthetic.orders) select * from tmp") == "SYNTHETIC.ORDERS","CTE aliases excluded");
        check(sqlReferences("select q'[secret ' FROM PASSWORD]' from \"Synthetic\".\"View\"") == "\"Synthetic\".\"View\"","Oracle q literal omitted; quoted identifiers preserved");
        check(sqlReferences("select * from a; select * from b@synthetic_link") == "A, B@SYNTHETIC_LINK","multiple statements and database links");
        std::cout<<"PASS "<<assertions<<" core assertions; synthetic artifacts: "<<root.string()<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}
}
