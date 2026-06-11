#pragma once

#include "types.h"
#include <string>

namespace pino {

struct EngineConfig;

// Loads EngineConfig from config.ini (desktop) or returns defaults.
// Checks config.json (logged as unsupported), then config.ini.
EngineConfig load_config();

// Parse a single INI-style config file into EngineConfig.
// Missing/invalid fields produce warnings but do not abort.
EngineConfig load_config_ini(const char* path);

} // namespace pino
