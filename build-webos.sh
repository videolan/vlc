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
MODE="${1:-all}"
SDK_DOWNLOAD_DIR="${SDK_DOWNLOAD_DIR:-$BUILD_DIR/sdk}"
SDK_ARCHIVE="${WEBOS_SDK_ARCHIVE:-}"
DEFAULT_WEBOS_SDK_URL="https://github.com/openlgtv/buildroot-nc4/releases/download/webos-a38c582/arm-webos-linux-gnueabi_sdk-buildroot-x86_64.tar.gz"
SDK_URL="${WEBOS_SDK_URL:-$DEFAULT_WEBOS_SDK_URL}"
AUTO_SDK_DOWNLOAD="${AUTO_SDK_DOWNLOAD:-1}"
DEFAULT_WEBOS_CONTRIB_BOOTSTRAP_FLAGS="--disable-disc --disable-sout --disable-basu --disable-flac --disable-vorbis --disable-fluid --disable-libaribcaption --disable-ass --disable-live555 --disable-harfbuzz --disable-vulkan-loader --disable-sidplay2 --disable-vncclient"
DEFAULT_WEBOS_CONFIGURE_EXTRA_FLAGS="--disable-libass --disable-libdrm --disable-qt --disable-vpx --disable-aom --disable-fluidsynth --disable-openapv"
WEBOS_CONTRIB_BOOTSTRAP_FLAGS="${WEBOS_CONTRIB_BOOTSTRAP_FLAGS:-$DEFAULT_WEBOS_CONTRIB_BOOTSTRAP_FLAGS}"
WEBOS_CONFIGURE_EXTRA_FLAGS="${WEBOS_CONFIGURE_EXTRA_FLAGS:-$DEFAULT_WEBOS_CONFIGURE_EXTRA_FLAGS}"

usage() {
        cat <<EOF
Usage: $0 [sdk|deps|configure|build|install|all]

Environment variables:
    WEBOS_TOOLCHAIN   Path to webOS ARM SDK/toolchain root (required if not auto-detected)
    TARGET            Target triplet (default: arm-webos-linux-gnueabi)
    BUILD_DIR         Out-of-tree VLC build directory (default: ~/vlc-webos-build)
    DEPS_PREFIX       Contrib install prefix (default: ~/vlc-webos-deps)
    DEPLOY_DIR        Install DESTDIR for packaged runtime tree (default: <vlc-src>/vlc-webos-deploy)
    PREFIX            VLC install prefix inside webOS app sandbox
    APP_ID            webOS app id (default: org.videolan.vlc)
    JOBS              Parallel jobs (default: nproc)
    SDK_DOWNLOAD_DIR  Directory for downloaded/extracted SDK (default: <BUILD_DIR>/sdk)
    WEBOS_SDK_ARCHIVE Local SDK tarball path (optional)
    WEBOS_SDK_URL     SDK tarball URL to download if SDK missing (default: openlgtv buildroot-nc4 webOS release)
    AUTO_SDK_DOWNLOAD Auto-download SDK when missing in configure/build/install/all (default: 1)
    WEBOS_CONTRIB_BOOTSTRAP_FLAGS Extra flags passed to contrib/bootstrap (default: reproducible webOS profile)
    WEBOS_CONFIGURE_EXTRA_FLAGS   Extra flags appended to VLC configure invocation (default: reproducible webOS profile)

Examples:
    WEBOS_SDK_URL=https://github.com/openlgtv/buildroot-nc4/releases/download/webos-a38c582/arm-webos-linux-gnueabi_sdk-buildroot-x86_64.tar.gz $0 sdk
    WEBOS_TOOLCHAIN=/opt/ndk $0 deps
    WEBOS_TOOLCHAIN=/opt/ndk $0 configure
    WEBOS_TOOLCHAIN=/opt/ndk $0 build
    WEBOS_TOOLCHAIN=/opt/ndk $0 install
    WEBOS_TOOLCHAIN=/opt/ndk PREFIX=/ $0 all
    make -C ~/vlc-webos-build -j$(nproc)
EOF
}

case "$MODE" in
    sdk|deps|configure|build|install|all)
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

has_sysroot_runtime() {
    local toolchain="$1"
    local sysroot="${toolchain}/${TARGET}/sysroot"
    [ -f "${sysroot}/usr/lib/crt1.o" ] && [ -f "${sysroot}/usr/lib/libc.so" ]
}

normalize_sysroot_libtool_archives() {
    local la
    while IFS= read -r -d '' la; do
        sed -i \
            -e "s|^libdir='/.*sysroot/usr/lib'|libdir='${SYSROOT}/usr/lib'|" \
            -e "s|/__w/buildroot-nc4/buildroot-nc4/output/host/${TARGET}/sysroot/usr/lib|${SYSROOT}/usr/lib|g" \
            -e "s|/__w/buildroot-nc4/buildroot-nc4/output/host/bin/../${TARGET}/sysroot/usr/lib|${SYSROOT}/usr/lib|g" \
            -e "s|/__w/buildroot-nc4/buildroot-nc4/output/host/${TARGET}/sysroot/usr/include|${SYSROOT}/usr/include|g" \
            -e "s|/__w/buildroot-nc4/buildroot-nc4/output/host/${TARGET}/lib|${WEBOS_TOOLCHAIN}/${TARGET}/lib|g" \
            -e "s|${WEBOS_TOOLCHAIN}/lib|${WEBOS_TOOLCHAIN}/${TARGET}/lib|g" \
            "$la" || true
    done < <(find "${SYSROOT}/usr/lib" "${DEPS_PREFIX}/lib" -maxdepth 1 -type f -name '*.la' -print0 2>/dev/null)
}

sanitize_makefile_paths() {
    local makefile="$1"
    [ -f "$makefile" ] || return 0
    sed -i \
        -e "s|-I/usr/include/libpng16|-I${SYSROOT}/usr/include/libpng16|g" \
        -e "s|-I/usr/include/freetype2|-I${SYSROOT}/usr/include/freetype2|g" \
        -e "s|-I/usr/include/libxml2|-I${SYSROOT}/usr/include/libxml2|g" \
        -e "s|-L/usr/lib\\>|-L${SYSROOT}/usr/lib|g" \
        "$makefile" || true
}

download_file() {
    local url="$1"
    local out="$2"

    if command -v curl >/dev/null 2>&1; then
        curl -fL "$url" -o "$out"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$out" "$url"
    else
        echo "Neither curl nor wget is available for SDK download."
        exit 1
    fi
}

detect_toolchain() {
    local candidate

    if [ -n "${WEBOS_TOOLCHAIN:-}" ] && [ -d "${WEBOS_TOOLCHAIN}" ] && has_sysroot_runtime "${WEBOS_TOOLCHAIN}"; then
        return 0
    fi

    WEBOS_TOOLCHAIN=""
    for candidate in \
        "$SDK_DOWNLOAD_DIR/arm-webos-linux-gnueabi_sdk-buildroot" \
        "$SDK_DOWNLOAD_DIR/arm-webos-linux-gnueabi_sdk-buildroot-x86_64"; do
        if [ -d "$candidate" ] && has_sysroot_runtime "$candidate"; then
            WEBOS_TOOLCHAIN="$candidate"
            return 0
        fi
    done

    candidate="$(find "$SDK_DOWNLOAD_DIR" -maxdepth 1 -type d -name 'arm-webos-linux-gnueabi_sdk-buildroot*' 2>/dev/null | head -n 1)"
    if [ -n "$candidate" ] && has_sysroot_runtime "$candidate"; then
        WEBOS_TOOLCHAIN="$candidate"
        return 0
    fi

    return 1
}

install_sdk_if_needed() {
    local archive_path=""
    local archive_name=""

    mkdir -p "$SDK_DOWNLOAD_DIR"

    if [ -n "$SDK_ARCHIVE" ] && [ -f "$SDK_ARCHIVE" ]; then
        archive_path="$SDK_ARCHIVE"
    elif [ -n "$SDK_URL" ]; then
        archive_name="$(basename "$SDK_URL")"
        archive_path="$SDK_DOWNLOAD_DIR/$archive_name"
        if [ ! -f "$archive_path" ]; then
            echo "Downloading webOS SDK: $SDK_URL"
            download_file "$SDK_URL" "$archive_path"
        else
            echo "Using existing SDK archive: $archive_path"
        fi
    else
        echo "SDK is missing and no download source was provided."
        echo "Set WEBOS_SDK_URL or WEBOS_SDK_ARCHIVE, or export WEBOS_TOOLCHAIN manually."
        exit 1
    fi

    echo "Extracting SDK archive to $SDK_DOWNLOAD_DIR"
    tar -xf "$archive_path" -C "$SDK_DOWNLOAD_DIR"
}

if ! detect_toolchain; then
    if [ "$MODE" = "sdk" ] || [ "$AUTO_SDK_DOWNLOAD" = "1" ]; then
        install_sdk_if_needed
    fi
fi

if ! detect_toolchain; then
    echo "WEBOS_TOOLCHAIN is not set and no usable SDK was found."
    echo "Set WEBOS_TOOLCHAIN to a toolchain containing ${TARGET}/sysroot/usr/lib/crt1.o."
    echo "Or run: WEBOS_SDK_URL=<sdk-archive-url> $0 sdk"
    exit 1
fi

if [ "$MODE" = "sdk" ]; then
    echo "SDK ready: ${WEBOS_TOOLCHAIN}"
    exit 0
fi

export PATH="/usr/bin:${WEBOS_TOOLCHAIN}/bin:${PATH}"
SYSROOT="${WEBOS_TOOLCHAIN}/${TARGET}/sysroot"
export PKG_CONFIG_SYSROOT_DIR=""
export PKG_CONFIG_LIBDIR="${DEPS_PREFIX}/lib/pkgconfig:${DEPS_PREFIX}/share/pkgconfig:${SYSROOT}/usr/lib/pkgconfig:${SYSROOT}/usr/share/pkgconfig"
export PKG_CONFIG_PATH="${DEPS_PREFIX}/lib/pkgconfig:${DEPS_PREFIX}/share/pkgconfig"
export CPPFLAGS="${CPPFLAGS:-} -I${DEPS_PREFIX}/include -I${SYSROOT}/usr/include"
export CFLAGS="${CFLAGS:-} -march=armv7-a -mfloat-abi=softfp -mfpu=neon -I${DEPS_PREFIX}/include"
export CXXFLAGS="${CXXFLAGS:-} -march=armv7-a -mfloat-abi=softfp -mfpu=neon -I${DEPS_PREFIX}/include"
export LDFLAGS="${LDFLAGS:-} -L${DEPS_PREFIX}/lib -L${SYSROOT}/usr/lib"

normalize_sysroot_libtool_archives

if ! command -v "${TARGET}-gcc" >/dev/null 2>&1; then
    echo "${TARGET}-gcc was not found in PATH."
    echo "Check WEBOS_TOOLCHAIN: ${WEBOS_TOOLCHAIN}"
    exit 1
fi

if [ "$MODE" = "deps" ] || [ "$MODE" = "all" ]; then
    mkdir -p "${SRC_DIR}/contrib/${TARGET}"
    cd "${SRC_DIR}/contrib/${TARGET}"

    if [ ! -f Makefile ]; then
        # Default to maximum contrib coverage; caller can still pass disable flags.
        # shellcheck disable=SC2086
        ../bootstrap --host="${TARGET}" --prefix="${DEPS_PREFIX}" ${WEBOS_CONTRIB_BOOTSTRAP_FLAGS}
    fi

    make fetch
    make -j"${JOBS}"
fi

if [ "$MODE" = "configure" ] || [ "$MODE" = "all" ]; then
    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"

    # Keep defaults broad and rely on auto-detection whenever possible.
    # shellcheck disable=SC2086
    "${SRC_DIR}/configure" \
        --host="${TARGET}" \
        --prefix="${PREFIX}" \
        --disable-debug \
        --disable-dbus \
        --disable-xcb \
        --without-x \
        --enable-run-as-root \
        ${WEBOS_CONFIGURE_EXTRA_FLAGS}

    if [ -f Makefile ]; then
        sed -i '/^LIBS = /{ /-ldl/! s/$/ -ldl/; }' Makefile || true
        sanitize_makefile_paths Makefile
    fi

    if [ -f modules/Makefile ]; then
        sed -i '/^LIBS = /{ /-ldl/! s/$/ -ldl/; }' modules/Makefile || true
        sed -i '/^LIBS_fluidsynth = /{ / -lm/! s/$/ -lm/; }' modules/Makefile || true
        sed -i 's/^am__append_351 = libdrm_display_plugin.la/# am__append_351 = libdrm_display_plugin.la/' modules/Makefile || true
        sed -i 's/^am__append_284 = libegl_display_generic_plugin.la/# am__append_284 = libegl_display_generic_plugin.la/' modules/Makefile || true
        sed -i 's/^am__append_120 = libgstdecode_plugin.la/# am__append_120 = libgstdecode_plugin.la/' modules/Makefile || true
        sed -i 's/^am__append_212 = libgst_mem_plugin.la/# am__append_212 = libgst_mem_plugin.la/' modules/Makefile || true
        sanitize_makefile_paths modules/Makefile
    fi

    if [ -f bin/Makefile ]; then
        sanitize_makefile_paths bin/Makefile
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
