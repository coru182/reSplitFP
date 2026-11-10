#!/usr/bin/env bash
# reSplitFP setup: the pinned dependencies, then the game's code recompiled from your own default.xbe.
#
#   ./setup.sh /path/to/default.xbe        then ./build-ps5.sh
#   ./setup.sh --toolchain                 only the dependencies and the PS5 toolchain (no default.xbe needed)
#
# deps/        Athelas 1.8.2 (its SDK: XboxXBE-Recomp's recompiler and runtime), glslang 15.4.0, Dear ImGui 1.91.0,
#              toml++ 3.4.0, and reUndr-ps5 with its PS5 toolchain (PayloadSDK, RADV built for the PS5, the packaging
#              tool)
# work/game/   Athelas's game template and SDK with game/ and patches/ over them, your default.xbe in assets/, and
#              generated/game: the C++ the recompiler makes from it
# Run it again with another default.xbe to recompile; what was downloaded and built is kept.
# REUNDR=/path/to/reUndr-ps5 uses a reUndr-ps5 checkout whose toolchain is already built (that build takes an hour
# or more and needs Docker).
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
DEPS=$ROOT/deps
XBE=${1:-}
if [ "$XBE" != --toolchain ]; then
    if [ ! -f "$XBE" ]; then
        echo "usage: ./setup.sh /path/to/default.xbe   (from your own disc of TimeSplitters: Future Perfect, US Xbox)"
        echo "       ./setup.sh --toolchain            (the dependencies and the PS5 toolchain only)"
        exit 2
    fi
    XBE=$(cd "$(dirname "$XBE")" && pwd)/$(basename "$XBE")
fi
JOBS=${JOBS:-$(nproc)}
export PATH=/usr/lib/llvm-18/bin:$PATH
for tool in git cmake ninja clang-18 clang++-18 ld.lld llvm-objcopy-18 llvm-objdump-18 python3 patch zip; do
    command -v "$tool" >/dev/null || { echo "setup: $tool is missing (README.md, Building)"; exit 1; }
done

# a repository at a pinned commit: name, url, commit
pin() {
    local dir=$DEPS/$1
    [ -d "$dir/.git" ] || git clone --filter=blob:none --no-checkout "$2" "$dir"
    git -C "$dir" checkout --quiet --detach "$3" 2>/dev/null ||
        { git -C "$dir" fetch --quiet origin && git -C "$dir" checkout --quiet --detach "$3"; }
}
mkdir -p "$DEPS"
pin Athelas https://github.com/Shagg88/Athelas.git f16c73e5ec248c9e9a5b3f1f0707c527e3719c7a          # 1.8.2
pin glslang https://github.com/KhronosGroup/glslang.git 8a85691a0740d390761a1008b4696f57facd02c4      # 15.4.0
pin imgui https://github.com/ocornut/imgui.git 8199457a7d9e453f8d3d9cadc14683fb54a858b5              # v1.91.0
pin tomlplusplus https://github.com/marzer/tomlplusplus.git 30172438cee64926dc41fdd9c11fb3ba5b2ba9de # v3.4.0

# reUndr-ps5's pinned PS5 toolchain, built by its own scripts (README.md there, "For developers")
REUNDR=${REUNDR:-$DEPS/reUndr-ps5}
[ "$REUNDR" != "$DEPS/reUndr-ps5" ] ||
    pin reUndr-ps5 https://github.com/bystritskiy/reUndr-ps5.git 91de6198be34c956069e9b5d9ce00aed532a73ab
VK=$REUNDR/build/deps/PS5_Vulkan
if [ ! -f "$VK/.deps/native/radv-release/lib/libvulkan_radeon.ps5.a" ] || [ ! -f "$VK/runtime/libc.prx" ] ||
   [ ! -f "$REUNDR/build/deps/compiler-rt/libclang_rt.builtins-x86_64.a" ]; then
    command -v docker >/dev/null || { echo "setup: building reUndr-ps5's toolchain needs Docker"; exit 1; }
    (cd "$REUNDR" && python3 tools/rebuild-deps.py --llvm-bin /usr/lib/llvm-18/bin --jobs "$JOBS")
fi
if [ ! -x "$REUNDR/build/ps5-native-tool" ]; then
    # reUndr's script names macOS's Homebrew clang: the same script, with this machine's clang++-18 (build/ sits
    # beside tools/, so it finds its files the same way)
    mkdir -p "$REUNDR/build"
    sed "s#/opt/homebrew/opt/llvm@18/bin/clang++#$(command -v clang++-18)#" "$REUNDR/tools/build-native-tool.py" \
        > "$REUNDR/build/build-native-tool-linux.py"
    (cd "$REUNDR" && python3 build/build-native-tool-linux.py)
fi
if [ "$XBE" = --toolchain ]; then
    echo "setup: the toolchain is ready"
    exit 0
fi

# the game's tree: Athelas's game template and its SDK, with this port's game files (game/: the Future Perfect
# config and hooks) and SDK changes (patches/: the PS5's, then the TimeSplitters ports': XDK 4721 and 5849, Direct3D,
# sound, input and video calls, visibility tests in Vulkan)
G=$ROOT/work/game
rm -rf "$G"
mkdir -p "$G/assets"
cp -r "$DEPS/Athelas/sdk/templates/game/." "$G/"
cp -r "$DEPS/Athelas/sdk" "$G/sdk"
cp -r "$ROOT/game/." "$G/"
patch -s -d "$G" -p1 < "$ROOT/patches/athelas-ps5.patch"
patch -s -d "$G" -p1 < "$ROOT/patches/tsfp-sdk.patch"
cp "$XBE" "$G/assets/default.xbe"

# the recompiler, then the game's C++ from default.xbe
T=$ROOT/work/tools
cmake -S "$ROOT/tools/recomp" -B "$T" -G Ninja -DCMAKE_BUILD_TYPE=Release -DXBE_SDK="$G/sdk" \
    -DCMAKE_C_COMPILER=clang-18 -DCMAKE_CXX_COMPILER=clang++-18 > /dev/null
cmake --build "$T" --target xbe_recomp -j "$JOBS"
(cd "$G" && "$T/xbe_recomp" config/game.toml --xdk sdk/xdk)
echo "setup: done; now ./build-ps5.sh"
