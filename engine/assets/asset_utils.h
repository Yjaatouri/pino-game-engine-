#pragma once

#include "engine/core/types.h"
#include <string>

namespace pino {

// Normalizes a file path for use as a consistent cache key:
//   - converts backslashes to forward slashes
//   - collapses ".", ".." segments
//   - lowercases on case-insensitive platforms (Win32)
std::string normalize_asset_path(const char* path);

} // namespace pino
