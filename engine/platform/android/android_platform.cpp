#include "engine/platform/platform.h"
#include "android_window.h"
#include "android_input.h"
#include "android_file_system.h"

#if defined(__ANDROID__)

namespace pino {

std::unique_ptr<Window> create_window(const WindowConfig& config) {
    return std::make_unique<AndroidWindow>(config);
}

std::unique_ptr<Input> create_input() {
    return std::make_unique<AndroidInput>();
}

std::unique_ptr<FileSystem> create_android_file_system(AAssetManager* mgr) {
    return std::make_unique<AndroidFileSystem>(mgr);
}

} // namespace pino

#endif // __ANDROID__
