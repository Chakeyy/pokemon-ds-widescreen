#!/usr/bin/env bash
# Builds the standalone Android APK using melonDS's ARM64 JIT and the NEON software rasterizer (Path A).
#
# Usage:
#   tools/android/build_apk.sh [abi] [output.apk]
#
# Defaults:
#   abi: arm64-v8a (64-bit phones/handhelds)
#   output: pokemon-ds-wide-arm64-v8a.apk
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$HERE"

if [ -z "${ANDROID_NDK_ROOT:-}" ] && [ -n "${ANDROID_NDK_HOME:-}" ]; then
    ANDROID_NDK_ROOT="$ANDROID_NDK_HOME"
fi
if [ -z "${ANDROID_NDK_ROOT:-}" ]; then
    echo "error: ANDROID_NDK_ROOT or ANDROID_NDK_HOME is not set" >&2
    exit 1
fi
TOOLCHAIN="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake"
if [ ! -f "$TOOLCHAIN" ]; then
    echo "error: $TOOLCHAIN not found" >&2
    exit 1
fi

ABI="${1:-arm64-v8a}"
API="24"
REV="${REV:-r3}"
OUT_APK="${2:-pokemon-ds-wide-${REV}-${ABI}.apk}"

BUILD_DIR="build-android-${ABI}"
DEPS_DIR="android-deps-${ABI}"
SDL_VERSION="2.32.10"

mkdir -p "$DEPS_DIR"

# 1. Build SDL2 for Android
SDL_PREFIX="$HERE/$DEPS_DIR/sdl2"
if [ ! -f "$SDL_PREFIX/lib/libSDL2.so" ]; then
    echo "== Building SDL2 for Android ($ABI)"
    if [ ! -d "$DEPS_DIR/SDL2-$SDL_VERSION" ]; then
        curl -fsSL "https://github.com/libsdl-org/SDL/releases/download/release-$SDL_VERSION/SDL2-$SDL_VERSION.tar.gz" -o "$DEPS_DIR/sdl2.tar.gz"
        tar -xzf "$DEPS_DIR/sdl2.tar.gz" -C "$DEPS_DIR"
    fi
    cmake -S "$DEPS_DIR/SDL2-$SDL_VERSION" -B "$DEPS_DIR/build-sdl2" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_PLATFORM="android-$API" \
        -DCMAKE_INSTALL_PREFIX="$SDL_PREFIX" \
        -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF
    ninja -C "$DEPS_DIR/build-sdl2" install
fi

# 2. Build pokemon-ds-widescreen (libmain.so)
echo "== Building pokemon-ds-widescreen ($ABI)"
# Path A: CPU NEON software rasterizer (SSR_GPU3D=OFF), ARM64 JIT enabled (SSR_JIT=ON)
cmake -S . -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DANDROID_ABI="$ABI" \
    -DANDROID_PLATFORM="android-$API" \
    -DSDL2_DIR="$SDL_PREFIX/lib/cmake/SDL2" \
    -DSSR_JIT=ON \
    -DSSR_GPU3D=OFF \
    -DSSR_GENERATED_DIR="$HERE/tools/bw2/stubgen"

ninja -C "$BUILD_DIR" ssrecomp

# 3. Pack APK using python
echo "== Packaging APK: $OUT_APK"
KEY_PATH="${KEY_PATH:-$HERE/tools/android/signing-key.json}"
python3 tools/android/apk.py \
    tools/android/skeleton.apk \
    "$ABI" \
    "$BUILD_DIR/libmain.so" \
    "$SDL_PREFIX/lib/libSDL2.so" \
    "" \
    "$KEY_PATH" \
    "$OUT_APK"

echo "== Successfully built $OUT_APK"
