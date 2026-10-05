#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <functional>

namespace activity {
struct Settings {
    bool enabled = true, paused = false, files = true, objects = true, connection = true;
    bool history = false, csv = false, ics = false, aw = true, waka = false;
    std::string wakaKey;
    int heartbeat = 15, idle = 300, rotationMB = 8, retentionDays = 90, awPort = 5600;
    std::filesystem::path directory;
    void validate() const;
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
    std::function<void(const std::string&)> heartbeatDelivery;
    std::function<void(int64_t,const Context&)> contextDelivery;
    Engine(Settings settings, std::string session = newId());
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    void sample(Sample sample);
    void stop(int64_t now, const std::string& reason);
};
}
