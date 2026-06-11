#pragma once

#include "engine/platform/window.h"
#include "engine/platform/input.h"
#include "engine/platform/file_system.h"

#include <memory>
#include <string>

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#elif defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IOS
// iOS-specific includes if needed
#endif
#endif

namespace pino {

std::unique_ptr<Window>     create_window(const WindowConfig& config);
std::unique_ptr<Input>      create_input();

#if defined(__ANDROID__)
std::unique_ptr<FileSystem> create_android_file_system(AAssetManager* mgr);
#elif defined(__APPLE__) && TARGET_OS_IOS
std::unique_ptr<FileSystem> create_ios_file_system();
#else
std::unique_ptr<FileSystem> create_file_system(const std::string& base_path);
#endif

} // namespace pino
