#!/bin/bash
# ─── build_ios.sh ──────────────────────────────────────────────────────────────
# Build Pino Engine for iOS from the command line.
# No Xcode GUI required — uses xcodebuild with CMake-generated Xcode project.
#
# Prerequisites:
#   - Xcode installed (Command Line Tools sufficient)
#   - An Apple Developer signing identity (or set CODE_SIGN_IDENTITY="")
#   - An iOS device or simulator target
#
# Usage:
#   ./build_ios.sh [debug|release] [device|simulator]
#
# Examples:
#   ./build_ios.sh debug simulator    # Debug build for simulator
#   ./build_ios.sh release device     # Release build for physical device
#   ./build_ios.sh                    # Default: debug, device
# ────────────────────────────────────────────────────────────────────────────────
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build_ios"

# ─── Config ──────────────────────────────────────────────────────────────────
CONFIG="${1:-debug}"
TARGET="${2:-device}"

if [ "$CONFIG" != "debug" ] && [ "$CONFIG" != "release" ]; then
    echo "Error: config must be 'debug' or 'release', got '$CONFIG'"
    exit 1
fi

if [ "$TARGET" != "device" ] && [ "$TARGET" != "simulator" ]; then
    echo "Error: target must be 'device' or 'simulator', got '$TARGET'"
    exit 1
fi

# ─── Determine SDK and arch ──────────────────────────────────────────────────
if [ "$TARGET" == "device" ]; then
    SDK="iphoneos"
    ARCH="-arch arm64"
    CODE_SIGN="Apple Development"
else
    SDK="iphonesimulator"
    ARCH="-arch x86_64 -arch arm64"
    CODE_SIGN=""  # Simulator doesn't require signing
fi

BUILD_TYPE="Debug"
if [ "$CONFIG" == "release" ]; then
    BUILD_TYPE="Release"
fi

echo "═══ Building Pino Engine for iOS ═══"
echo "  Config:    $CONFIG ($BUILD_TYPE)"
echo "  Target:    $TARGET"
echo "  SDK:       $SDK"
echo "  Build dir: $BUILD_DIR"
echo ""

# ─── Step 1: CMake configure ─────────────────────────────────────────────────
echo "--- CMake configure ---"
cmake -B "$BUILD_DIR" \
    -G "Xcode" \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY="$CODE_SIGN" \
    -DCMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET="14.0" \
    "$PROJECT_DIR/ios"

# ─── Step 2: Build ───────────────────────────────────────────────────────────
echo ""
echo "--- Building ---"
cmake --build "$BUILD_DIR" \
    --config "$BUILD_TYPE" \
    -- -sdk "$SDK" $ARCH -parallelizeTargets

# ─── Step 3: Locate the .app bundle ──────────────────────────────────────────
echo ""
echo "--- Locating app bundle ---"
APP_PATH=$(find "$BUILD_DIR" -name "*.app" -type d | head -1)
if [ -z "$APP_PATH" ]; then
    echo "Error: .app bundle not found in $BUILD_DIR"
    exit 1
fi
echo "App bundle: $APP_PATH"

# ─── Step 4: Sign (device only) ──────────────────────────────────────────────
if [ "$TARGET" == "device" ]; then
    echo ""
    echo "--- Signing ---"
    codesign --force --sign "$CODE_SIGN" "$APP_PATH"
fi

# ─── Step 5: Generate IPA (device only) ──────────────────────────────────────
if [ "$TARGET" == "device" ]; then
    IPA_PATH="$BUILD_DIR/pino_engine-$CONFIG.ipa"
    echo ""
    echo "--- Generating IPA: $IPA_PATH ---"
    mkdir -p "$BUILD_DIR/Payload"
    cp -r "$APP_PATH" "$BUILD_DIR/Payload/"
    cd "$BUILD_DIR"
    zip -qr "$IPA_PATH" Payload/
    rm -rf Payload/
    echo "IPA: $IPA_PATH"
fi

echo ""
echo "═══ Build complete ═══"
