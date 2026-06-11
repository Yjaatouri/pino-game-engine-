#include "engine/platform/platform.h"
#include "ios_window.h"
#include "ios_input.h"
#include "ios_file_system.h"

namespace pino {

std::unique_ptr<Window> create_window(const WindowConfig& config) {
    return std::make_unique<IOSWindow>(config.existing_native_window);
}

std::unique_ptr<Input> create_input() {
    return std::make_unique<IOSInput>();
}

std::unique_ptr<FileSystem> create_ios_file_system() {
    return std::make_unique<IOSFileSystem>();
}

} // namespace pino
