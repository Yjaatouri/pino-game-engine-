#include "engine/platform/platform.h"
#include "sdl2_window.h"
#include "sdl2_input.h"
#include "sdl2_file_system.h"

#include <SDL.h>

namespace pino {

std::unique_ptr<Window> create_window(const WindowConfig& config) {
    return std::make_unique<Sdl2Window>(config);
}

std::unique_ptr<Input> create_input() {
    return std::make_unique<Sdl2Input>();
}

std::unique_ptr<FileSystem> create_file_system(const std::string& base_path) {
    return std::make_unique<Sdl2FileSystem>(base_path);
}

} // namespace pino
