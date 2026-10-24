#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <functional>
#include <array>

namespace activity {
enum ExtraField { App, WindowId, WindowType, WindowTypeName, FileKind, ObjectType, ObjectOwner, ObjectName, ConnectionAlias, IntervalId, DeliverySegmentId, ExtraFieldCount };
inline constexpr const char* extraFieldNames[]={"app","window_id","window_type","window_type_name","file_kind","object_type","object_owner","object_name","connection_alias","interval_id","delivery_segment_id"};
struct Settings {
    bool extended=false;
    std::array<bool,ExtraFieldCount> extraFields{true,true,true,true,true,false,false,false,false,true,true};
    bool enabled = true, paused = false, files = true, objects = true, connection = true;
    bool history = false, csv = false, ics = false, aw = true, waka = false;
    bool testing = false;
    std::string wakaKey;
    int heartbeat = 15, idle = 300, rotationMB = 8, retentionDays = 90, awPort = 5600;
    std::filesystem::path directory;
    void validate() const;
    int effectiveAwPort() const { return testing && awPort == 5600 ? 5666 : awPort; }
};
struct Context {
    std::string window, file, objectType, owner, objectName, connection;
    int windowType = 0;
    bool operator==(const Context& b) const;
};
std::string quote(const std::string& s);
std::string utc(int64_t milliseconds);
std::string newId();
struct Sample {
    int64_t utcMs = 0, monotonicMs = 0;
    bool focused = false, idle = false;
    Context context;
    bool discontinuity = false;
};
class Journal {
    Settings settings;
    std::string session;
    uint64_t sequence = 0;
public:
    Journal(Settings settings, std::string session);
    std::filesystem::path append(int64_t time, const std::string& record);
    void expire(int64_t now);
    void exportInterval(const std::string& id, int64_t start, int64_t end, const Context& context);
};
class Engine {
    Settings settings;
    Journal journal;
    std::string session, interval;
    Context context;
    bool active = false;
    int64_t start = 0, lastUtc = 0, lastMono = 0, pulseMono = 0;
    void write(const std::string& type, int64_t now, const std::string& extra = "");
    void finish(int64_t end, const std::string& reason);
public:
    bool deliveryBoundary=true;
    std::function<void(const std::string&)> heartbeatDelivery;
    std::function<void(int64_t,const Context&)> contextDelivery;
    Engine(Settings settings, std::string session = newId());
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    void sample(Sample sample);
    void stop(int64_t now, const std::string& reason);
};
}
