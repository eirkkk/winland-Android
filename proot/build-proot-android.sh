#!/bin/bash
# Build PRoot (Termux fork) + static talloc for Android arm64-v8a.
# Usage: ./build-proot-android.sh [path-to-ndk]  (default: /opt/android/ndk)
# Output: ./out/{proot,proot-loader,proot-loader32}
# NOTE: needs aarch64 libxml2.so.16 for the NDK linker — provided
# system-wide at /usr/lib/aarch64-linux-gnu/ (see docs/PROOT_MODE.md).
set -euo pipefail
cd "$(dirname "$0")"

NDK_HOME="${1:-${ANDROID_NDK_HOME:-/opt/android/ndk}}"
if [ -z "$NDK_HOME" ]; then
    echo "ERROR: pass NDK path as \$1 or set ANDROID_NDK_HOME"
    exit 1
fi
echo "Using NDK: $NDK_HOME"

# SnowNF ndk-aarch64-linux ships an x86_64 libxml2 that lld cannot load on
# aarch64 hosts. Use our locally built aarch64 libxml2.so.16 if present.
# Already cd'd to the script dir above (line: cd "$(dirname "$0")"), so the
# script dir is simply $PWD. (Resolving BASH_SOURCE[0] again would break
# when invoked via a relative path, since cwd already changed.)
SCRIPT_DIR="$PWD"
if [ -f "$SCRIPT_DIR/hostlibs/libxml2.so.16" ]; then
    export LD_LIBRARY_PATH="$SCRIPT_DIR/hostlibs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
fi

TOOLCHAIN="$NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64"
if [ ! -x "$TOOLCHAIN/bin/aarch64-linux-android${API:-26}-clang" ]; then
    # SnowNF ndk-aarch64-linux layout fallback: find the real prebuilt dir
    TOOLCHAIN="$(dirname "$(find "$NDK_HOME" \( -name 'aarch64-linux-android26-clang' -o -name 'aarch64-linux-android31-clang' \) 2>/dev/null | head -1)")"
    echo "Resolved toolchain dir: $TOOLCHAIN"
fi

API=26
TARGET="aarch64-linux-android"
CC="$TOOLCHAIN/bin/${TARGET}${API}-clang"
AR="$TOOLCHAIN/bin/llvm-ar"
STRIP="$TOOLCHAIN/bin/llvm-strip"

for tool in "$CC" "$AR" "$STRIP"; do
    [ -x "$tool" ] || { echo "ERROR: missing tool $tool"; ls "$TOOLCHAIN/bin" | head -30; exit 1; }
done
echo "CC=$CC"
"$CC" --version | head -1

if [ ! -f "proot-termux/src/GNUmakefile" ]; then
    echo "ERROR: proot-termux source missing. Clone https://github.com/termux/proot here."
    exit 1
fi
if ! ls talloc-2.4.2/talloc.c >/dev/null 2>&1; then
    if [ -f dl/talloc-2.4.2.tar.gz ]; then
        tar xzf dl/talloc-2.4.2.tar.gz
    else
        echo "ERROR: talloc source missing."
        exit 1
    fi
fi

BUILD_DIR="build-arm64-v8a"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
OUT="$PWD/out"
mkdir -p "$OUT"

# --- 1. talloc static lib (direct compile, bypass waf) ---
echo "=== Building talloc (static) ==="
TALLOC_INSTALL="$PWD/$BUILD_DIR/talloc-install"
mkdir -p "$TALLOC_INSTALL/lib" "$TALLOC_INSTALL/include"
cp -a talloc-2.4.2 "$BUILD_DIR/talloc-src"
    (
        cd "$BUILD_DIR/talloc-src"
        # NOTE: config.h is hand-written (see ../talloc-2.4.2/config.h) because
        # waf configure cannot run in this cross environment.
        $CC -c -I. -Ilib/replace \
            -DHAVE_CONFIG_H \
            -D__STDC_WANT_LIB_EXT1__=1 \
            -DTALLOC_BUILD_VERSION_MAJOR=2 \
            -DTALLOC_BUILD_VERSION_MINOR=4 \
            -DTALLOC_BUILD_VERSION_RELEASE=2 \
            -D_GNU_SOURCE \
            -fPIC \
            talloc.c -o talloc.o
    "$AR" rcs libtalloc.a talloc.o
    cp libtalloc.a "$TALLOC_INSTALL/lib/"
    cp talloc.h "$TALLOC_INSTALL/include/"
)
echo "talloc built."

# --- 2. TLS alignment fix object (Android 15 needs 64-byte TLS align on ARM64) ---
TLS_FIX="$PWD/$BUILD_DIR/tls_align.o"
echo '__thread int __tls_align_fix __attribute__((aligned(64))) = 0;' | \
    "$CC" -fno-emulated-tls -c -x c - -o "$TLS_FIX"
echo "TLS fix object built."

# --- 3. PRoot (Termux fork) ---
echo "=== Building PRoot (Termux fork) ==="
cp -a proot-termux "$BUILD_DIR/proot-src"
sed -i '1i #include <string.h>' "$BUILD_DIR/proot-src/src/extension/ashmem_memfd/ashmem_memfd.c" 2>/dev/null || true
(
    cd "$BUILD_DIR/proot-src/src"
    make -j"$(nproc)" \
        CC="$CC" \
        LD="$CC" \
        STRIP="$STRIP" \
        OBJCOPY="$TOOLCHAIN/bin/llvm-objcopy" \
        OBJDUMP="$TOOLCHAIN/bin/llvm-objdump" \
        CPPFLAGS="-D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -I. -I\$(VPATH) -I\$(VPATH)/../lib/uthash/include -I$TALLOC_INSTALL/include" \
        CFLAGS="-g -Wall -O2 -I$TALLOC_INSTALL/include" \
        LDFLAGS="-L$TALLOC_INSTALL/lib -ltalloc -static $TLS_FIX" \
        CARE_LDFLAGS="" \
        HAS_SWIG="" \
        HAS_PYTHON_CONFIG="" \
        V=1 \
        proot 2>&1 | tail -8
    "$STRIP" proot
    ls -la proot
)
cp "$BUILD_DIR/proot-src/src/proot" "$OUT/proot"
if [ -f "$BUILD_DIR/proot-src/src/loader/loader" ]; then
    "$STRIP" "$BUILD_DIR/proot-src/src/loader/loader"
    cp "$BUILD_DIR/proot-src/src/loader/loader" "$OUT/proot-loader"
fi
if [ -f "$BUILD_DIR/proot-src/src/loader/loader32" ]; then
    "$STRIP" "$BUILD_DIR/proot-src/src/loader/loader32" 2>/dev/null || true
    cp "$BUILD_DIR/proot-src/src/loader/loader32" "$OUT/proot-loader32" 2>/dev/null || true
fi

echo ""
echo "=== Done ==="
ls -la "$OUT/"
