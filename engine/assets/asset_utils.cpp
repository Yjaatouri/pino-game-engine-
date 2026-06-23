#include "asset_utils.h"
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>

namespace pino {

std::string normalize_asset_path(const char* path) {
    if (!path || !path[0]) return {};

    std::string p = path;
    for (auto& ch : p) if (ch == '\\') ch = '/';

    std::vector<std::string> segments;
    std::istringstream ss(p);
    std::string seg;
    while (std::getline(ss, seg, '/')) {
        if (seg.empty() || seg == ".") continue;
        if (seg == ".." && !segments.empty()) {
            segments.pop_back();
        } else if (seg != "..") {
            segments.push_back(seg);
        }
    }

    std::string result;
    for (usize i = 0; i < segments.size(); ++i) {
        if (i > 0) result += '/';
        result += segments[i];
    }

#if defined(_WIN32)
    for (auto& ch : result)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
#endif

    return result;
}

} // namespace pino
