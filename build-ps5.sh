#!/usr/bin/env bash
# reSplitFP: builds the PS5 app (PPSA99065) into build-ps5/PPSA99065-<version>.zip, after ./setup.sh, from
#   work/game      the TimeSplitters: Future Perfect port: its sources, config and the SDK (with the PS5 and the
#                  TimeSplitters ports' changes), and generated/game, the C++ xbe_recomp made from the maintainer's own default.xbe
#   ps5/           the PS5 host: app shell, entry, input, audio, the in-game menu, the first start's unpacking
#   package/       what the app carries besides the program: sce_sys (param.json, icon), licences, README.txt
#   art/           optional: your own icon0.png, pic0.dds, pic1.dds (tools/mkart.py), in place of package/sce_sys's
#   deps/          glslang, Dear ImGui, toml++, and reUndr-ps5's PS5 toolchain (PayloadSDK, RADV, the packaging tool)
# Stages: ./build-ps5.sh [compile] [link] [package]  (all three when none is named). JOBS=<n> compiles n at a time.
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
DEPS=$ROOT/deps
GAME=$ROOT/work/game
PKG=$ROOT/package
REUNDR=${REUNDR:-$DEPS/reUndr-ps5}
VK=$REUNDR/build/deps/PS5_Vulkan
SDK=$VK/.deps/native/ps5-payload-sdk
OUT=$ROOT/build-ps5
TITLE=PPSA99065
VERSION=0.1.5
JOBS=${JOBS:-$(nproc)}
export PATH=/usr/lib/llvm-18/bin:$PATH
CXX="$SDK/bin/prospero-clang++"
STAGES=("$@"); [ ${#STAGES[@]} -gt 0 ] || STAGES=(compile link package)
has() { local s; for s in "${STAGES[@]}"; do [ "$s" = "$1" ] && return 0; done; return 1; }

# glslang 15.4.0 (the version the PC port pins) with its HLSL front end, no optimizer: the runtime compiles the
# game's shaders from HLSL (reUndr's glslang is GLSL only)
GLSLANG_SRC=$DEPS/glslang
GLSLANG_BUILD=$OUT/glslang
IMGUI=$DEPS/imgui
[ -d "$GAME/generated/game" ] || { echo "build: run ./setup.sh /path/to/default.xbe first"; exit 1; }
INC=(-I"$GAME/sdk/runtime" -I"$GAME/src" -I"$GAME/generated/game" -I"$ROOT" -I"$GAME/sdk/harness/compat"
     -isystem "$GAME/sdk/third_party/vulkan/include" -isystem "$IMGUI" -isystem "$IMGUI/backends"
     -isystem "$DEPS/tomlplusplus/include" -isystem "$GLSLANG_SRC" -isystem "$GLSLANG_BUILD/include")
DEFS=(-DXBE_RENDER_THREAD=0 -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES -DIMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
      -DGAME_VERSION="\"$VERSION\"" -DTSFP_PS5_VERSION="\"$VERSION\"")
BASE=(-std=c++20 -fno-strict-aliasing -ffunction-sections -fdata-sections -fno-plt -femulated-tls)
mkdir -p "$OUT/gen" "$OUT/host"

if has compile; then
    # glslang for the PS5, once: HLSL front end on, its optimizer off
    if [ ! -f "$GLSLANG_BUILD/glslang/libglslang.a" ]; then
        cmake -S "$GLSLANG_SRC" -B "$GLSLANG_BUILD" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$REUNDR/ps5/cmake/ps5.cmake" \
            -DPS5_PAYLOAD_SDK="$SDK" -DCMAKE_C_COMPILER=/usr/lib/llvm-18/bin/clang \
            -DCMAKE_CXX_COMPILER=/usr/lib/llvm-18/bin/clang++ -DCMAKE_BUILD_TYPE=Release \
            -DCMAKE_CXX_FLAGS_RELEASE="-O2 -DNDEBUG -ffunction-sections -fdata-sections" -DENABLE_HLSL=ON \
            -DENABLE_OPT=OFF -DENABLE_GLSLANG_BINARIES=OFF -DENABLE_SPVREMAPPER=OFF -DGLSLANG_TESTS=OFF \
            -DGLSLANG_ENABLE_INSTALL=OFF -DENABLE_EXCEPTIONS=OFF -DENABLE_RTTI=OFF -DBUILD_EXTERNAL=OFF \
            -DBUILD_WERROR=OFF -DBUILD_SHARED_LIBS=OFF > /dev/null
        cmake --build "$GLSLANG_BUILD" --target glslang glslang-default-resource-limits -j "$JOBS"
    fi
    # the game's code, as the PC port compiles it (-O2), one object per partition; rebuilt only when newer
    export CXX OUT GAME
    ls "$GAME"/generated/game/x86_recomp.*.cpp "$GAME"/generated/game/x86_func_mapping.cpp | xargs -P "$JOBS" -I{} sh -c '
        o="$OUT/gen/$(basename {} .cpp).o"; [ "$o" -nt {} ] && exit 0
        $CXX -std=c++20 -O2 -g0 -w -fno-strict-aliasing -ffunction-sections -fdata-sections -fno-plt -femulated-tls \
            -I"$GAME/sdk/runtime" -I"$GAME/generated/game" -c {} -o "$o.tmp" && mv "$o.tmp" "$o" && echo "gen $(basename {})" || { echo "FAIL {}"; exit 255; }'
    # the PlayStation buttons and controller built in: game/art's drawings, or your own controller picture when
    # art/pad.png is there (tools/mkbuttons.py --image)
    python3 "$ROOT/tools/mkbuttons.py" --embed "$GAME/src/game/tsfp_button_art.cpp"
    # the host: the runtime without its Windows parts (app/, SDL input, Direct3D 12, XAudio2, the DLL upscalers),
    # the port's own code, ImGui, and the PS5 host
    SRC=$(ls "$GAME"/sdk/runtime/*/*.cpp | grep -Ev "/runtime/app/|/core/input.cpp|/gpu/d3d12.cpp|/gpu/upscale.cpp|/audio/xaudio2.cpp")
    SRC="$SRC $GAME/sdk/runtime/app/settings_list.cpp $(ls "$GAME"/src/*/*.cpp) $(ls "$ROOT"/ps5/*.cpp)"
    SRC="$SRC $IMGUI/imgui.cpp $IMGUI/imgui_draw.cpp $IMGUI/imgui_tables.cpp $IMGUI/imgui_widgets.cpp"
    SRC="$SRC $IMGUI/backends/imgui_impl_vulkan.cpp"
    # the release check's data (ps5/xbe_match.h): the build's default.xbe's layout and 256-byte blocks, hashed
    if [ ! "$OUT/host/xbe_blocks.cpp" -nt "$GAME/assets/default.xbe" ] || [ ! "$OUT/host/xbe_blocks.cpp" -nt "$ROOT/ps5/xbe_match.h" ]; then
        clang++-18 -std=c++20 -O2 -I"$ROOT" "$ROOT/tools/xbe_blocks.cpp" -o "$OUT/xbe_blocks"
        "$OUT/xbe_blocks" "$GAME/assets/default.xbe" "$OUT/host/xbe_blocks.cpp"
    fi
    SRC="$SRC $OUT/host/xbe_blocks.cpp"
    # an object is rebuilt when its source is newer, and the ones that print the version when the version changes
    stamp=$OUT/host/version.stamp
    [ "$(cat "$stamp" 2>/dev/null)" = "$VERSION" ] || echo "$VERSION" > "$stamp"
    fails=0
    for f in $SRC; do
        o=$OUT/host/$(echo "${f#$ROOT/}" | tr / _ | sed 's/\.cpp$/.o/')
        if [ "$o" -nt "$f" ] && { [ "$o" -nt "$stamp" ] || ! grep -q "GAME_VERSION\|TSFP_PS5_VERSION" "$f"; }; then continue; fi
        ( "$CXX" "${BASE[@]}" -O2 -g -w "${DEFS[@]}" "${INC[@]}" -c "$f" -o "$o" || { echo "FAIL $f"; exit 1; } ) &
        [ "$(jobs -r | wc -l)" -ge "$JOBS" ] && { wait -n || fails=$((fails + 1)); }
    done
    while [ "$(jobs -r | wc -l)" -gt 0 ]; do wait -n || fails=$((fails + 1)); done
    [ $fails -eq 0 ] || { echo "$fails host sources failed"; exit 1; }
    for f in "$ROOT"/ps5/*.c; do
        o=$OUT/host/$(echo "${f#$ROOT/}" | tr / _ | sed 's/\.c$/.o/')
        [ "$o" -nt "$f" ] || "$SDK/bin/prospero-clang" -std=c11 -O2 -ffunction-sections -fdata-sections -fno-plt -I"$ROOT" -c "$f" -o "$o" || exit 1
    done
    echo "compile: done"
fi

if has link; then
    # reUndr's link (tools/link.sh, MIT; the RADV recipe and native runtime are GPL-3.0-or-later): RADV linked in
    # whole, the payload SDK's libc++/libc++abi/libunwind, its system module stubs, one export map
    work=$OUT/link; mkdir -p "$work/obj" "$work/stubs"
    archive=$VK/.deps/native/radv-release/lib/libvulkan_radeon.ps5.a
    # the recipe takes compiler-rt's builtins from clang's resource folder: reUndr's pinned copy goes there (with sudo
    # when that folder belongs to root, as on a CI runner)
    resource=$(clang-18 --print-resource-dir); rt="$REUNDR/build/deps/compiler-rt/libclang_rt.builtins-x86_64.a"
    if ! cmp -s "$rt" "$resource/lib/linux/libclang_rt.builtins-x86_64.a"; then
        { mkdir -p "$resource/lib/linux" && cp "$rt" "$resource/lib/linux/"; } 2>/dev/null ||
            sudo sh -c "mkdir -p '$resource/lib/linux' && cp '$rt' '$resource/lib/linux/'"
    fi
    for pair in 'libSceAgc agc_canary_link_stub.c' 'libSceAgcDriver agc_driver_canary_link_stub.c'; do
        read -r library source <<<"$pair"
        "$SDK/bin/prospero-clang" -std=c11 -O2 -fPIC -c "$VK/vendor/ps5/sdk/stubs/$source" -o "$work/obj/$library.o"
        "$SDK/bin/prospero-lld" --shared -soname "$library.prx" -o "$work/stubs/$library.so" "$work/obj/$library.o"
    done
    export LLVM_CONFIG=/usr/bin/llvm-config-18 PS5_CLANG=/usr/bin/clang-18
    source "$VK/tools/radv-link.sh"
    radv_link_recipe "$VK" "$SDK" "$archive"
    inputs=(); for item in "${radv_link_inputs[@]}"; do case "$item" in --start-group|--end-group) ;; *) inputs+=("$item") ;; esac; done
    flags=(); skip=0
    for item in "${radv_link_flags[@]}"; do
        if ((skip)); then skip=0; continue; fi
        if [[ $item == --version-script ]]; then skip=1; continue; fi
        flags+=("$item")
    done
    # libc.a's raw-syscall readlink, pathconf and fork share an object the link needs: bound to ps5/posix_ps5.c's
    for name in readlink pathconf fork; do flags+=("--defsym=$name=tsfp_$name"); done
    { printf '{local:\n_Zn*; _Zd*;\n'
      for item in "${flags[@]}"; do [[ $item == --defsym=* ]] && { n=${item#--defsym=}; printf '%s;\n' "${n%%=*}"; }; done
      printf '*;};\n'; } > "$work/exports.map"
    shaders=("$GLSLANG_BUILD/glslang/libglslang.a" "$GLSLANG_BUILD/glslang/libglslang-default-resource-limits.a")
    stubs=(); for f in "$SDK"/target/lib/*.so; do case "$f" in *libScePosixForWebKit.so|*libSceGLSlimVSH.so|*libc_stub_weak.so) ;; *) stubs+=("$f") ;; esac; done
    "$SDK/bin/prospero-lld" "${radv_linker_script[@]}" "${flags[@]}" --version-script "$work/exports.map" \
        --exclude-libs=ALL --no-dynamic-linker --gc-sections -e _start -o "$work/llvm-pie.elf" \
        "$work/stubs/libSceAgc.so" "$work/stubs/libSceAgcDriver.so" \
        --start-group "$OUT"/host/*.o "$OUT"/gen/*.o "${shaders[@]}" "${inputs[@]}" \
        "$SDK/target/lib/libc.a" "$SDK/target/lib/libpthread.a" --end-group --as-needed "${stubs[@]}" > "$work/link.log" 2>&1 || true
    grep -E "error|undefined" "$work/link.log" | head -40 || true
    [ -s "$work/llvm-pie.elf" ] && [ "$work/llvm-pie.elf" -nt "$work/exports.map" ] || { echo "link: failed (see $work/link.log)"; exit 1; }
    # a PS5 title may not issue a syscall itself (SYSTEM_ILLEGAL_FUNCTION_CALL): none may be left in the program
    llvm-objdump-18 -d --no-show-raw-insn "$work/llvm-pie.elf" | awk '/^[0-9a-f]+ <.*>:$/ {fn=$2} /\tsyscall/ {print fn}' \
        | sort | uniq -c > "$work/raw_syscalls.txt"
    if [ -s "$work/raw_syscalls.txt" ]; then echo "link: raw syscalls left in:"; cat "$work/raw_syscalls.txt"; exit 1; fi
    echo "link: no raw syscalls"
    echo "link: $(du -h "$work/llvm-pie.elf" | cut -f1) $work/llvm-pie.elf"
fi

if has package; then
    # reUndr's tools/package.py steps: module info and imports, a signed fake SELF, the PS5 libc module, sce_sys
    work=$OUT/link; native=$REUNDR/build/ps5-native-tool; app=$OUT/app/$TITLE
    [ -x "$native" ] || { echo "package: the native tool is not built ($native)"; exit 1; }
    rm -rf "$app"; mkdir -p "$app/sce_module"
    # the console gets the program without debug info; llvm-pie.elf keeps it for reading crash addresses
    llvm-objcopy-18 --strip-debug "$work/llvm-pie.elf" "$work/llvm-pie.stripped.elf"
    "$native" link --in "$work/llvm-pie.stripped.elf" --out "$work/eboot.elf" --stub-dir "$SDK/target/lib" \
        --stub "$work/stubs/libSceAgc.so" --stub "$work/stubs/libSceAgcDriver.so" --module-sdk 0x02000009 \
        --companion-sdk 0x08050001 --file-name eboot.elf > "$work/convert.log" 2>&1
    "$native" self --sign --in "$work/eboot.elf" --out "$app/eboot.bin" --magic 0x1D3D154F > "$work/sign.log" 2>&1
    cp "$VK/runtime/libc.prx" "$app/sce_module/libc.prx"
    for f in "$app/eboot.bin" "$app/sce_module/libc.prx"; do
        "$native" self --inspect --file "$f" | grep -q "integrity: valid" || { echo "package: $f is not a valid fake SELF"; exit 1; }
    done
    cp -r "$PKG/sce_sys" "$app/sce_sys"
    for f in icon0.png pic0.dds pic1.dds; do if [ -f "$ROOT/art/$f" ]; then cp "$ROOT/art/$f" "$app/sce_sys/$f"; fi; done
    mkdir -p "$app/licenses" && cp "$PKG"/licenses/* "$app/licenses/"
    cp "$PKG/README.txt" "$app/README.txt"
    (cd "$OUT/app" && rm -f "../$TITLE-$VERSION.zip" && zip -qr "../$TITLE-$VERSION.zip" "$TITLE")
    echo "package: $OUT/$TITLE-$VERSION.zip"
fi
