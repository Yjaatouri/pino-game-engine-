#include "config_loader.h"
#include "engine/engine.h"
#include "log.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace pino {

static void trim(std::string& s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.erase(s.begin());
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))  s.pop_back();
}

static bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
            return false;
    }
    return true;
}

static LogLevel parse_log_level(const std::string& val) {
    if (iequals(val, "debug")) return LogLevel::Debug;
    if (iequals(val, "info"))  return LogLevel::Info;
    if (iequals(val, "warn"))  return LogLevel::Warn;
    if (iequals(val, "error")) return LogLevel::Error;
    return LogLevel::Debug; // default
}

EngineConfig load_config_ini(const char* path) {
    EngineConfig cfg;

    FILE* f = std::fopen(path, "r");
    if (!f) {
        PINO_WARN("Config file not found: %s — using defaults", path);
        return cfg;
    }

    PINO_INFO("Loading config: %s", path);

    char line[256];
    int line_num = 0;
    while (std::fgets(line, sizeof(line), f)) {
        ++line_num;
        std::string s(line);

        // Strip trailing newline
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();

        // Skip empty / comment lines
        if (s.empty() || s[0] == '#' || s[0] == ';') continue;

        auto eq = s.find('=');
        if (eq == std::string::npos) {
            PINO_WARN("config.ini:%d: skipping malformed line", line_num);
            continue;
        }

        std::string key = s.substr(0, eq);
        std::string val = s.substr(eq + 1);
        trim(key);
        trim(val);

        if (key.empty()) {
            PINO_WARN("config.ini:%d: empty key", line_num);
            continue;
        }

        // Match known keys
        if (iequals(key, "width")) {
            int w = std::atoi(val.c_str());
            if (w > 0) cfg.window_width = static_cast<u32>(w);
            else PINO_WARN("config.ini:%d: invalid width '%s'", line_num, val.c_str());
        } else if (iequals(key, "height")) {
            int h = std::atoi(val.c_str());
            if (h > 0) cfg.window_height = static_cast<u32>(h);
            else PINO_WARN("config.ini:%d: invalid height '%s'", line_num, val.c_str());
        } else if (iequals(key, "title")) {
            // Store in static buffer since EngineConfig uses const char*
            static std::string s_title;
            s_title = val;
            cfg.app_title = s_title.c_str();
        } else if (iequals(key, "fullscreen")) {
            cfg.fullscreen = iequals(val, "true") || iequals(val, "1") || iequals(val, "yes");
        } else if (iequals(key, "resizable")) {
            cfg.resizable = iequals(val, "true") || iequals(val, "1") || iequals(val, "yes");
        } else if (iequals(key, "vsync")) {
            cfg.vsync = iequals(val, "true") || iequals(val, "1") || iequals(val, "yes");
        } else if (iequals(key, "log_level")) {
            cfg.log_level = parse_log_level(val);
        } else if (iequals(key, "target_fps")) {
            int fps = std::atoi(val.c_str());
            if (fps > 0) cfg.fixed_update_rate = static_cast<u32>(fps);
            else PINO_WARN("config.ini:%d: invalid target_fps '%s'", line_num, val.c_str());
        } else {
            PINO_WARN("config.ini:%d: unknown key '%s'", line_num, key.c_str());
        }
    }

    std::fclose(f);
    return cfg;
}

EngineConfig load_config() {
    // Try config.json first (not yet supported — log a note)
    FILE* test = std::fopen("config.json", "r");
    if (test) {
        std::fclose(test);
        PINO_WARN("config.json found but JSON config loading is not yet supported — using defaults");
        EngineConfig cfg;
        cfg.app_title = "Pino Engine (defaults)";
        return cfg;
    }

    // Try config.ini
    test = std::fopen("config.ini", "r");
    if (test) {
        std::fclose(test);
        return load_config_ini("config.ini");
    }

    // No config file — use defaults
    EngineConfig cfg;
    PINO_INFO("No config file found — using built-in defaults");
    return cfg;
}

} // namespace pino
