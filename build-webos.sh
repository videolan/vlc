#!/usr/bin/env bash

set -euo pipefail

SRC_DIR="$(cd "$(dirname "$0")" && pwd)"
TARGET="${TARGET:-arm-webos-linux-gnueabi}"
APP_ID="${APP_ID:-org.videolan.vlc}"
PREFIX="${PREFIX:-/media/developer/apps/usr/palm/applications/${APP_ID}}"
BUILD_DIR="${BUILD_DIR:-$HOME/vlc-webos-build}"
DEPS_PREFIX="${DEPS_PREFIX:-$HOME/vlc-webos-deps}"
DEPLOY_DIR="${DEPLOY_DIR:-$SRC_DIR/vlc-webos-deploy}"
JOBS="${JOBS:-$(nproc)}"

has_sysroot_runtime() {
    local toolchain="$1"
    local sysroot="${toolchain}/${TARGET}/sysroot"
    [ -f "${sysroot}/usr/lib/crt1.o" ] && [ -f "${sysroot}/usr/lib/libc.so" ]
}

if [ -z "${WEBOS_TOOLCHAIN:-}" ]; then
    for candidate in \
        "$HOME/kodi-dev/arm-webos-linux-gnueabi_sdk-buildroot" \
        "$HOME/buildroot-nc4/output/host"; do
        if [ -d "$candidate" ] && has_sysroot_runtime "$candidate"; then
            WEBOS_TOOLCHAIN="$candidate"
            break
        fi
    done
fi

if [ -z "${WEBOS_TOOLCHAIN:-}" ]; then
    echo "WEBOS_TOOLCHAIN is not set and no usable SDK was found."
    echo "Set WEBOS_TOOLCHAIN to a toolchain containing ${TARGET}/sysroot/usr/lib/crt1.o."
    exit 1
fi

if ! has_sysroot_runtime "${WEBOS_TOOLCHAIN}"; then
    echo "Toolchain is missing runtime sysroot files (crt1.o/libc.so): ${WEBOS_TOOLCHAIN}"
    exit 1
fi

usage() {
    cat <<EOF
Usage: $0 [deps|configure|build|install|all]

Environment variables:
  WEBOS_TOOLCHAIN   Path to webOS ARM SDK/toolchain root (required if not auto-detected)
  TARGET            Target triplet (default: arm-webos-linux-gnueabi)
  BUILD_DIR         Out-of-tree VLC build directory (default: ~/vlc-webos-build)
  DEPS_PREFIX       Contrib install prefix (default: ~/vlc-webos-deps)
    DEPLOY_DIR        Install DESTDIR for packaged runtime tree (default: <vlc-src>/vlc-webos-deploy)
  PREFIX            VLC install prefix inside webOS app sandbox
  APP_ID            webOS app id (default: org.videolan.vlc)
  JOBS              Parallel jobs (default: nproc)

Examples:
  WEBOS_TOOLCHAIN=/opt/ndk $0 deps
  WEBOS_TOOLCHAIN=/opt/ndk $0 configure
    WEBOS_TOOLCHAIN=/opt/ndk $0 build
    WEBOS_TOOLCHAIN=/opt/ndk $0 install
    WEBOS_TOOLCHAIN=/opt/ndk PREFIX=/ $0 all
    make -C ~/vlc-webos-build -j$(nproc)
EOF
}

MODE="${1:-all}"
case "$MODE" in
        deps|configure|build|install|all)
        ;;
    -h|--help|help)
        usage
        exit 0
        ;;
    *)
        usage
        exit 1
        ;;
esac

export PATH="/usr/bin:${WEBOS_TOOLCHAIN}/bin:${PATH}"
SYSROOT="${WEBOS_TOOLCHAIN}/${TARGET}/sysroot"
unset PKG_CONFIG_SYSROOT_DIR
export PKG_CONFIG_LIBDIR="${DEPS_PREFIX}/lib/pkgconfig:${DEPS_PREFIX}/share/pkgconfig:${SYSROOT}/usr/lib/pkgconfig:${SYSROOT}/usr/share/pkgconfig"
export PKG_CONFIG_PATH="${DEPS_PREFIX}/lib/pkgconfig:${DEPS_PREFIX}/share/pkgconfig"
export CFLAGS="${CFLAGS:-} -march=armv7-a -mfloat-abi=softfp -mfpu=neon"
export CXXFLAGS="${CXXFLAGS:-} -march=armv7-a -mfloat-abi=softfp -mfpu=neon"

if ! command -v "${TARGET}-gcc" >/dev/null 2>&1; then
    echo "${TARGET}-gcc was not found in PATH."
    echo "Check WEBOS_TOOLCHAIN: ${WEBOS_TOOLCHAIN}"
    exit 1
fi

if [ "$MODE" = "deps" ] || [ "$MODE" = "all" ]; then
    mkdir -p "${SRC_DIR}/contrib/${TARGET}"
    cd "${SRC_DIR}/contrib/${TARGET}"

    if [ ! -f Makefile ]; then
        ../bootstrap --host="${TARGET}" --prefix="${DEPS_PREFIX}" --disable-disc --disable-sout
    fi

    make fetch
    make -j"${JOBS}"
fi

if [ "$MODE" = "configure" ] || [ "$MODE" = "all" ]; then
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"

    "${SRC_DIR}/configure" \
        --host="${TARGET}" \
        --prefix="${PREFIX}" \
        --disable-debug \
        --disable-dbus \
        --disable-xcb \
        --disable-wayland \
        --disable-libdrm \
        --disable-egl \
        --disable-gles2 \
        --disable-vdpau \
        --disable-gst-decode \
        --disable-aribsub \
        --disable-aribcaption \
        --disable-lua \
        --disable-freetype \
        --disable-libass \
        --disable-qt \
        --disable-skins2 \
        --disable-udev \
        --disable-avahi \
        --without-x \
        --enable-run-as-root

    if [ -f Makefile ]; then
        sed -i '/^LIBS = /{ /-ldl/! s/$/ -ldl/; }' Makefile || true
    fi

    if [ -f modules/Makefile ]; then
        sed -i '/^LIBS = /{ /-ldl/! s/$/ -ldl/; }' modules/Makefile || true
        sed -i 's/^am__append_351 = libdrm_display_plugin.la/#&/' modules/Makefile || true
        sed -i 's/^am__append_207 = libfreetype_plugin.la/#&/' modules/Makefile || true
    fi

    if [ -f bin/Makefile ]; then
        if ! grep -q 'rpath-link,\$(abs_top_builddir)/src/.libs' bin/Makefile; then
            sed -i '/^vlc_CPPFLAGS =/a vlc_LDFLAGS = $(LDFLAGS_vlc) -Wl,-rpath-link,$(abs_top_builddir)/src/.libs -Wl,-rpath-link,$(abs_top_builddir)/lib/.libs' bin/Makefile || true
        fi
        sed -i 's|^\(vlc_cache_gen_LDADD = .*../lib/libvlc.la\)\(.*\)$|\1 \\\n\t../src/libvlccore.la\2|' bin/Makefile || true
    fi

    if [ -f config.h ] && [ ! -f "${DEPS_PREFIX}/include/idna.h" ] && [ ! -f "${SYSROOT}/usr/include/idna.h" ]; then
        sed -i 's/^#define HAVE_IDN 1$/\/\* #undef HAVE_IDN \*\//g' config.h || true
    fi

    echo "webOS configure complete in ${BUILD_DIR}"
    echo "If linking fails with libvlccore/libglibc_polyfills issues, adjust bin/Makefile and LIBS as described in extras/buildsystem/README.webOS.md."
    echo "Next: make -C ${BUILD_DIR} -j${JOBS}"
fi

if [ "$MODE" = "build" ] || [ "$MODE" = "all" ]; then
    if [ ! -f "${BUILD_DIR}/Makefile" ]; then
        echo "Missing ${BUILD_DIR}/Makefile. Run '$0 configure' first."
        exit 1
    fi
    make -C "${BUILD_DIR}" -j"${JOBS}"
fi

if [ "$MODE" = "install" ] || [ "$MODE" = "all" ]; then
    if [ ! -f "${BUILD_DIR}/Makefile" ]; then
        echo "Missing ${BUILD_DIR}/Makefile. Run '$0 configure' first."
        exit 1
    fi
    mkdir -p "${DEPLOY_DIR}"
    make -C "${BUILD_DIR}" install DESTDIR="${DEPLOY_DIR}"
    echo "Installed runtime tree to ${DEPLOY_DIR}"
fi
