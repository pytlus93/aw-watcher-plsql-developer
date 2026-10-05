#include "core.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <stdexcept>
#include <tuple>
#define NOMINMAX
#include <windows.h>

namespace activity {
void Settings::validate() const {
    if (history && !directory.is_absolute()) throw std::runtime_error("Destination must be an absolute directory.");
    if (heartbeat < 1 || heartbeat > 300) throw std::runtime_error("Heartbeat must be 1 to 300 seconds.");
    if (idle < heartbeat || idle > 86400) throw std::runtime_error("Idle must be at least heartbeat and at most 86400 seconds.");
    if (rotationMB < 1 || rotationMB > 1024) throw std::runtime_error("Rotation must be 1 to 1024 MB.");
    if (retentionDays < 1 || retentionDays > 3650) throw std::runtime_error("Retention must be 1 to 3650 days.");
    if (awPort < 1 || awPort > 65535) throw std::runtime_error("ActivityWatch port must be 1 to 65535.");
}
bool Context::operator==(const Context& b) const {
    return std::tie(window,file,objectType,owner,objectName,connection,windowType) ==
        std::tie(b.window,b.file,b.objectType,b.owner,b.objectName,b.connection,b.windowType);
}
std::string quote(const std::string& s) {
    std::ostringstream o; o << '"';
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') o << '\\' << c;
        else if (c < 32) o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)c << std::dec;
        else o << c;
    }
    o << '"'; return o.str();
}
std::string utc(int64_t milliseconds) {
    time_t t = static_cast<time_t>(milliseconds / 1000); tm v{};
    gmtime_s(&v, &t);
    std::ostringstream o; o << std::put_time(&v, "%Y-%m-%dT%H:%M:%S") << '.'
        << std::setw(3) << std::setfill('0') << milliseconds % 1000 << 'Z'; return o.str();
}
std::string newId() {
    std::random_device r; std::ostringstream o;
    for (int i=0;i<4;i++) o << std::hex << std::setw(8) << std::setfill('0') << r();
    return o.str();
}
Journal::Journal(Settings s, std::string id): settings(s), session(id) { settings.validate(); }
std::filesystem::path Journal::append(int64_t time, const std::string& record) {
    auto root = settings.directory / "plsql-activity-v1";
    std::filesystem::create_directories(root);
    auto prefix = "activity-" + utc(time).substr(0,10) + "-" + session + "-";
    std::filesystem::path path;
    do {
        path = root / (prefix + std::to_string(sequence) + ".jsonl");
        if (!std::filesystem::exists(path) || std::filesystem::file_size(path) + record.size() + 1 <=
            static_cast<uint64_t>(settings.rotationMB)*1024*1024) break;
        ++sequence;
    } while (true);
    auto line=record+'\n';
    HANDLE file=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot open activity journal; recording stopped.");
    DWORD written=0;
    bool ok=WriteFile(file,line.data(),static_cast<DWORD>(line.size()),&written,nullptr)&&written==line.size();
    if(ok) ok=FlushFileBuffers(file)!=0;
    CloseHandle(file);
    if(!ok) throw std::runtime_error("Cannot flush activity journal; recording stopped.");
    return path;
}
void Journal::expire(int64_t now) {
    auto root = settings.directory / "plsql-activity-v1";
    if (!std::filesystem::exists(root)) return;
    auto cutoff = utc(now - static_cast<int64_t>(settings.retentionDays)*86400000).substr(0,10);
    for (const auto& entry : std::filesystem::directory_iterator(root)) {
        if (entry.is_symlink() || !entry.is_regular_file()) continue;
        auto name = entry.path().filename().string();
        // Only our date-prefixed files in our dedicated namespace; never recurse.
        if (name.rfind("activity-",0) != 0 && name.rfind("interval-",0) != 0) continue;
        auto date = name.substr(9,10);
        if (date.size()!=10 || date[4]!='-' || date[7]!='-') continue;
        if (entry.path().extension() != ".jsonl" && entry.path().extension() != ".csv" && entry.path().extension() != ".ics") continue;
        if (date < cutoff) std::filesystem::remove(entry.path());
    }
}
static std::string csvCell(std::string s) {
    // Prevent spreadsheet formula injection; preserve quotes/newlines with RFC4180 quoting.
    if (!s.empty() && (s[0]=='=' || s[0]=='+' || s[0]=='-' || s[0]=='@' || s[0]=='\t' || s[0]=='\r')) s = "'" + s;
    std::string result="\"";
    for (char c:s) { if(c=='"') result+='"'; result+=c; }
    return result+'"';
}
static std::string icsText(const std::string& s) {
    std::string result;
    for (char c:s) { if(c=='\r') continue; if(c=='\n') result+="\\n"; else { if(c=='\\'||c==';'||c==',') result+='\\'; result+=c; } }
    return result;
}
static std::string icsTime(int64_t n) {
    auto t=utc(n); std::string r;
    for(size_t i=0;i<19;i++) if(t[i]!='-'&&t[i]!=':') r+=t[i];
    return r+"Z";
}
static void folded(std::ostream& o, const std::string& line) {
    size_t offset=0;
    while(offset<line.size()) {
        size_t count=std::min<size_t>(offset?74:75,line.size()-offset);
        // UTF-8 continuation bytes must stay with their leading byte.
        while(count>0 && offset+count<line.size() && (static_cast<unsigned char>(line[offset+count])&0xc0)==0x80) --count;
        if(offset) o << ' '; o << line.substr(offset,count) << "\r\n"; offset+=count;
    }
}
void Journal::exportInterval(const std::string& id, int64_t start, int64_t end, const Context& c) {
    auto root=settings.directory/"plsql-activity-v1";
    auto prefix="interval-"+utc(start).substr(0,10)+"-"+id;
    if(settings.csv) {
        std::ofstream o(root/(prefix+".csv"),std::ios::binary);
        o << "id,start_utc,end_utc,duration_seconds,window_id,file,object_owner,object_name,connection_alias\r\n";
        o << csvCell(id)<<','<<csvCell(utc(start))<<','<<csvCell(utc(end))<<','<<std::fixed<<std::setprecision(3)<<(end-start)/1000.0<<','<<csvCell(c.window)<<','<<csvCell(c.file)<<','<<csvCell(c.owner)<<','<<csvCell(c.objectName)<<','<<csvCell(c.connection)<<"\r\n";
        o.close();if(!o) throw std::runtime_error("CSV export failed; completed interval remains in JSONL.");
    }
    if(settings.ics) {
        std::ofstream o(root/(prefix+".ics"),std::ios::binary);
        o << "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//Local Activity//PLSQL v1//EN\r\nBEGIN:VEVENT\r\n";
        folded(o,"UID:"+id+"@plsql-activity.local"); folded(o,"DTSTAMP:"+icsTime(end));
        folded(o,"DTSTART:"+icsTime(start)); folded(o,"DTEND:"+icsTime(end));
        folded(o,"SUMMARY:PL/SQL Developer");
        folded(o,"DESCRIPTION:"+icsText(c.file+" "+c.owner+" "+c.objectName));
        o << "END:VEVENT\r\nEND:VCALENDAR\r\n";
        o.close();if(!o) throw std::runtime_error("ICS export failed; completed interval remains in JSONL.");
    }
}
static std::string data(const Context& c) {
    return "{\"app\":\"PL/SQL Developer\",\"language\":\"PL/SQL\",\"window_id\":" + quote(c.window) +
        ",\"window_type\":" + std::to_string(c.windowType) + ",\"file\":" + quote(c.file) +
        ",\"object_type\":" + quote(c.objectType) + ",\"object_owner\":" + quote(c.owner) +
        ",\"object_name\":" + quote(c.objectName) + ",\"connection_alias\":" + quote(c.connection) + "}";
}
Engine::Engine(Settings s,std::string id):settings(s),journal(s,id),session(id) {}
void Engine::write(const std::string& type,int64_t now,const std::string& extra) {
    if(!settings.history) return;
    journal.append(now,"{\"schema_version\":1,\"id\":"+quote(newId())+",\"type\":"+quote(type)+
        ",\"timestamp\":"+quote(utc(now))+",\"session_id\":"+quote(session)+",\"interval_id\":"+quote(interval)+
        ",\"context\":"+data(context)+extra+"}");
}
void Engine::finish(int64_t end,const std::string& reason) {
    if(!active) return;
    end=std::max(start,end);
    write("interval",end,",\"start\":"+quote(utc(start))+",\"end\":"+quote(utc(end))+
        ",\"duration_ms\":"+std::to_string(end-start)+",\"reason\":"+quote(reason));
    if(heartbeatDelivery&&end>start) {
        auto payload=data(context);payload.pop_back();payload+=",\"interval_id\":"+quote(interval)+"}";
        heartbeatDelivery("{\"timestamp\":"+quote(utc(end))+",\"duration\":0,\"data\":"+payload+"}");
    }
    active=false;
    if(settings.history&&end>start) journal.exportInterval(interval,start,end,context);
}
void Engine::sample(Sample s) {
    if(!settings.files) s.context.file.clear();
    if(!settings.objects) {s.context.objectType.clear();s.context.owner.clear();s.context.objectName.clear();}
    if(!settings.connection) s.context.connection.clear();
    bool gap=active && (s.discontinuity || s.monotonicMs-lastMono>3000 ||
        s.monotonicMs<lastMono || s.utcMs<lastUtc || std::abs((s.utcMs-lastUtc)-(s.monotonicMs-lastMono))>2000);
    if(gap) {finish(lastUtc,"gap");write("gap",s.utcMs);}
    bool recording=settings.enabled&&!settings.paused&&s.focused&&!s.idle&&!s.context.window.empty();
    if(active && (!recording || !(context==s.context)))
        finish(s.utcMs,!settings.enabled||settings.paused?"paused":!s.focused?"focus_lost":s.idle?"idle":"context_changed");
    if(recording&&!active) {
        context=s.context;interval=newId();start=s.utcMs;active=true;pulseMono=s.monotonicMs-settings.heartbeat*1000;
        write("start",s.utcMs);if(settings.history) journal.expire(s.utcMs);
    }
    if(active&&s.monotonicMs-pulseMono>=settings.heartbeat*1000) {
        write("heartbeat",s.utcMs);pulseMono=s.monotonicMs;
        if(contextDelivery) contextDelivery(s.utcMs,context);
        if(heartbeatDelivery) {
            auto payload=data(context);payload.pop_back();payload+=",\"interval_id\":"+quote(interval)+"}";
            heartbeatDelivery("{\"timestamp\":"+quote(utc(s.utcMs))+",\"duration\":0,\"data\":"+payload+"}");
        }
    }
    lastUtc=s.utcMs;lastMono=s.monotonicMs;
}
void Engine::stop(int64_t now,const std::string& reason) {
    finish(std::min(now,lastUtc+1000),reason);
}
}
