#!/usr/bin/env bash

set -euo pipefail

SRC_DIR="$(cd "$(dirname "$0")/../../.." && pwd)"
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
WEBOS_QT6_ROOT="${WEBOS_QT6_ROOT:-}"
WEBOS_QT6_HOST_TOOLS="${WEBOS_QT6_HOST_TOOLS:-}"
WEBOS_QT6_TARGET_PREFIX="${WEBOS_QT6_TARGET_PREFIX:-}"
WEBOS_QT6_VERSION="${WEBOS_QT6_VERSION:-6.8.3}"
WEBOS_QT6_AUTO_DOWNLOAD="${WEBOS_QT6_AUTO_DOWNLOAD:-1}"
WEBOS_QT6_SRC_DIR="${WEBOS_QT6_SRC_DIR:-$HOME/qt6-webos-src-$WEBOS_QT6_VERSION}"
WEBOS_QT6_BUILD_DIR="${WEBOS_QT6_BUILD_DIR:-$HOME/qt6-webos-build-host-$WEBOS_QT6_VERSION}"
WEBOS_QT6_HOST_PREFIX="${WEBOS_QT6_HOST_PREFIX:-$HOME/qt6-webos/host}"
WEBOS_QT6_CMAKE_GENERATOR="${WEBOS_QT6_CMAKE_GENERATOR:-Ninja}"
WEBOS_QT6_HOST_MODULES="${WEBOS_QT6_HOST_MODULES:-qtdeclarative,qtshadertools,qttools}"
WEBOS_QT6_TARGET_BUILD_DIR="${WEBOS_QT6_TARGET_BUILD_DIR:-$HOME/qt6-webos-build-target-$WEBOS_QT6_VERSION}"
WEBOS_QT6_TARGET_MODULES="${WEBOS_QT6_TARGET_MODULES:-qtshadertools,qtdeclarative,qtwayland}"
WEBOS_QT6_TARGET_TOOLCHAIN_FILE="${WEBOS_QT6_TARGET_TOOLCHAIN_FILE:-}"
WEBOS_DISABLE_FREETYPE_ON_MISSING_HB="${WEBOS_DISABLE_FREETYPE_ON_MISSING_HB:-1}"

DEFAULT_WEBOS_CONTRIB_BOOTSTRAP_FLAGS="--disable-disc --disable-sout --disable-basu --disable-flac --disable-libaribcaption --disable-ass --disable-vulkan-loader --disable-sidplay2 --disable-vncclient --enable-fluidlite --enable-live555"
DEFAULT_WEBOS_CONFIGURE_EXTRA_FLAGS="--disable-libass --disable-vpx --disable-aom --enable-fluidsynth --disable-openapv --disable-vdpau --disable-caca"
WEBOS_CONTRIB_BOOTSTRAP_FLAGS="${WEBOS_CONTRIB_BOOTSTRAP_FLAGS:-$DEFAULT_WEBOS_CONTRIB_BOOTSTRAP_FLAGS}"
WEBOS_CONFIGURE_EXTRA_FLAGS="${WEBOS_CONFIGURE_EXTRA_FLAGS:-$DEFAULT_WEBOS_CONFIGURE_EXTRA_FLAGS}"

if ! printf '%s' "$WEBOS_CONFIGURE_EXTRA_FLAGS" | grep -Eq '(^|[[:space:]])--(enable|disable)-qt($|[[:space:]])'; then
    WEBOS_CONFIGURE_EXTRA_FLAGS="${WEBOS_CONFIGURE_EXTRA_FLAGS} --enable-qt"
fi

QT_ENABLED=1
if printf '%s' "$WEBOS_CONFIGURE_EXTRA_FLAGS" | grep -Eq '(^|[[:space:]])--disable-qt($|[[:space:]])'; then
    QT_ENABLED=0
fi

usage() {
        cat <<EOF
Usage: $0 [qt6|qt6-host-tools|qt6-target-arm|sdk|deps|configure|build|install|all]

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
    WEBOS_QT6_ROOT                Optional external Qt6 SDK root for cross builds.
    WEBOS_QT6_HOST_TOOLS          Optional path to host Qt6 tools dir containing qmake6/moc/rcc/uic/qsb/qmlcachegen.
    WEBOS_QT6_TARGET_PREFIX       Optional target Qt6 prefix (contains lib/pkgconfig, include, qml, plugins).
    WEBOS_QT6_VERSION             Qt6 version used for source download bootstrap (default: 6.8.3).
    WEBOS_QT6_AUTO_DOWNLOAD       Auto-download Qt6 sources when ARM Qt is enabled but qmake6 is missing (default: 1).
    WEBOS_QT6_SRC_DIR             Directory used by the Qt6 downloader (default: ~/qt6-webos-src-<version>).
    WEBOS_QT6_BUILD_DIR           Host Qt6 tools build directory (default: ~/qt6-webos-build-host-<version>).
    WEBOS_QT6_HOST_PREFIX         Install prefix for host Qt6 tools (default: ~/qt6-webos/host).
    WEBOS_QT6_CMAKE_GENERATOR     CMake generator for Qt host tools build (default: Ninja).
    WEBOS_QT6_HOST_MODULES        Extra Qt6 host modules to build (comma-separated; default: qtdeclarative,qtshadertools,qttools).
    WEBOS_QT6_TARGET_BUILD_DIR    ARM Qt6 target build directory (default: ~/qt6-webos-build-target-<version>).
    WEBOS_QT6_TARGET_MODULES      ARM Qt6 target modules to build (comma-separated; default: qtshadertools,qtdeclarative,qtwayland).
    WEBOS_QT6_TARGET_TOOLCHAIN_FILE Optional CMake toolchain file for ARM Qt6 target build.
    WEBOS_DISABLE_FREETYPE_ON_MISSING_HB Disable freetype plugin in generated modules/Makefile when hb-ft.h is missing (default: 1).

Examples:
    WEBOS_SDK_URL=https://github.com/openlgtv/buildroot-nc4/releases/download/webos-a38c582/arm-webos-linux-gnueabi_sdk-buildroot-x86_64.tar.gz $0 sdk
    WEBOS_TOOLCHAIN=/opt/ndk $0 deps
    WEBOS_TOOLCHAIN=/opt/ndk $0 configure
    WEBOS_QT6_VERSION=6.8.3 $0 qt6
    WEBOS_QT6_VERSION=6.8.3 $0 qt6-host-tools
    WEBOS_QT6_VERSION=6.8.3 $0 qt6-target-arm
    WEBOS_TOOLCHAIN=/opt/ndk WEBOS_QT6_ROOT=$HOME/qt6-webos $0 configure
    WEBOS_TOOLCHAIN=/opt/ndk WEBOS_QT6_HOST_TOOLS=$HOME/qt6-webos/host/bin WEBOS_QT6_TARGET_PREFIX=$HOME/qt6-webos/target/usr $0 configure
    WEBOS_TOOLCHAIN=/opt/ndk $0 build
    WEBOS_TOOLCHAIN=/opt/ndk $0 install
    WEBOS_TOOLCHAIN=/opt/ndk PREFIX=/ $0 all
    make -C ~/vlc-webos-build -j$(nproc)
EOF
}

download_qt6_sources() {
    local downloader="$SRC_DIR/extras/buildsystem/download-qt6-webos.sh"

    if [ ! -x "$downloader" ]; then
        echo "Missing downloader script: $downloader"
        echo "Expected at: extras/buildsystem/download-qt6-webos.sh"
        exit 1
    fi

    QT_VERSION="$WEBOS_QT6_VERSION" OUT_DIR="$WEBOS_QT6_SRC_DIR" "$downloader"
}

extract_qt6_module() {
    local module="$1"
    local archive="$WEBOS_QT6_SRC_DIR/${module}-everywhere-src-${WEBOS_QT6_VERSION}.tar.xz"
    local extracted="$WEBOS_QT6_SRC_DIR/${module}-everywhere-src-${WEBOS_QT6_VERSION}"

    if [ -d "$extracted" ]; then
        printf '%s\n' "$extracted"
        return 0
    fi

    if [ ! -f "$archive" ]; then
        echo "Missing archive: $archive"
        echo "Run: $0 qt6"
        exit 1
    fi

    tar -xf "$archive" -C "$WEBOS_QT6_SRC_DIR"
    printf '%s\n' "$extracted"
}

apply_qt6_webos_compat_patches() {
    local qtbase_src="$1"
    local file="$qtbase_src/src/corelib/io/qstorageinfo_linux.cpp"
    local elf_file="$qtbase_src/src/corelib/plugin/qelfparser_p.cpp"

    [ -f "$file" ] || return 0

    python3 - "$file" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
text = path.read_text(encoding="utf-8")

changed = False

if "#include <sys/statvfs.h>" not in text and "#include <sys/statfs.h>" in text:
    text = text.replace("#include <sys/statfs.h>\n", "#include <sys/statfs.h>\n#include <sys/statvfs.h>\n", 1)
    changed = True

old = "        readOnly = (statfs_buf.f_flags & ST_RDONLY) != 0;\n"
new = (
    "        struct statvfs statvfs_buf;\n"
    "        if (statvfs(QFile::encodeName(rootPath).constData(), &statvfs_buf) == 0)\n"
    "            readOnly = (statvfs_buf.f_flag & ST_RDONLY) != 0;\n"
    "        else\n"
    "            readOnly = false;\n"
)

if old in text and "statvfs_buf.f_flag" not in text:
    text = text.replace(old, new, 1)
    changed = True

if changed:
    path.write_text(text, encoding="utf-8")
PY

    [ -f "$elf_file" ] || return 0

    python3 - "$elf_file" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
text = path.read_text(encoding="utf-8")

old = "    case EM_AARCH64:    d << \", AArch64\"; break;\n"
new = (
    "#ifdef EM_AARCH64\n"
    "    case EM_AARCH64:    d << \", AArch64\"; break;\n"
    "#endif\n"
)

if old in text and "#ifdef EM_AARCH64" not in text:
    text = text.replace(old, new, 1)
    path.write_text(text, encoding="utf-8")
PY
}

apply_qt6_qtdeclarative_compat_patches() {
    local qtdeclarative_src="$1"
    local file="$qtdeclarative_src/src/CMakeLists.txt"

    [ -f "$file" ] || return 0

    python3 - "$file" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
text = path.read_text(encoding="utf-8")

marker = "if(TARGET Qt::Gui AND TARGET Qt::qsb AND QT_FEATURE_qml_animation)"
inject = (
    "find_package(Qt6 ${PROJECT_VERSION} QUIET CONFIG OPTIONAL_COMPONENTS Gui)\n"
    "\n"
    "if(NOT TARGET Qt::qsb AND DEFINED ENV{QSB} AND EXISTS \"$ENV{QSB}\")\n"
    "    add_executable(Qt::qsb IMPORTED GLOBAL)\n"
    "    set_target_properties(Qt::qsb PROPERTIES IMPORTED_LOCATION \"$ENV{QSB}\")\n"
    "endif()\n\n"
)

if marker in text and inject not in text:
    text = text.replace(marker, inject + marker, 1)
    path.write_text(text, encoding="utf-8")
PY

    # Patch tools/CMakeLists.txt: skip host-only tools (qmltime, qml2puppet, etc.)
    # that require matching host binaries which may not be present in a minimal
    # qt6-host-tools build done via qt-configure-module.
    local tools_cmake="$qtdeclarative_src/tools/CMakeLists.txt"
    [ -f "$tools_cmake" ] || return 0

    python3 - "$tools_cmake" <<'PY'
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
text = path.read_text(encoding="utf-8")

# Tools whose host counterpart is not built by the minimal qt6-host-tools phase.
# They are only needed for development/testing on desktop - not for ARM target libs.
skip_tools = ["qmltime", "qml2puppet", "qmlprofiler", "qmlplugindump"]

changed = False
for tool in skip_tools:
    old = "add_subdirectory({tool})\n".format(tool=tool)
    new = "# add_subdirectory({tool})  # patched: host tool not in minimal qt6-host-tools build\n".format(tool=tool)
    if old in text and new not in text:
        text = text.replace(old, new, 1)
        changed = True

if changed:
    path.write_text(text, encoding="utf-8")
PY
}

build_qt6_host_tools() {
    if ! command -v cmake >/dev/null 2>&1; then
        echo "cmake was not found in PATH."
        exit 1
    fi

    if [ "$WEBOS_QT6_CMAKE_GENERATOR" = "Ninja" ] && ! command -v ninja >/dev/null 2>&1; then
        echo "ninja was not found in PATH (required for WEBOS_QT6_CMAKE_GENERATOR=Ninja)."
        exit 1
    fi

    [ -d "$WEBOS_QT6_SRC_DIR" ] || mkdir -p "$WEBOS_QT6_SRC_DIR"
    download_qt6_sources

    local qtbase_src
    qtbase_src="$(extract_qt6_module qtbase)"

    mkdir -p "$WEBOS_QT6_BUILD_DIR" "$WEBOS_QT6_HOST_PREFIX"

    cmake -S "$qtbase_src" -B "$WEBOS_QT6_BUILD_DIR" \
        -G "$WEBOS_QT6_CMAKE_GENERATOR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$WEBOS_QT6_HOST_PREFIX" \
        -DQT_BUILD_EXAMPLES=OFF \
        -DQT_BUILD_TESTS=OFF \
        -DBUILD_SHARED_LIBS=ON

    cmake --build "$WEBOS_QT6_BUILD_DIR" --parallel "$JOBS"

    if ! cmake --install "$WEBOS_QT6_BUILD_DIR"; then
        echo "cmake --install failed after partial host-tools build; using manual tool staging fallback."

        mkdir -p "$WEBOS_QT6_HOST_PREFIX/bin" "$WEBOS_QT6_HOST_PREFIX/lib" "$WEBOS_QT6_HOST_PREFIX/libexec"

        stage_tool() {
            local tool_name="$1"
            local src_path

            if [ -e "$WEBOS_QT6_HOST_PREFIX/bin/$tool_name" ]; then
                return 0
            fi

            src_path="$(find "$WEBOS_QT6_BUILD_DIR" -maxdepth 6 -type f -name "$tool_name" -perm -111 2>/dev/null | head -n 1)"
            if [ -n "$src_path" ]; then
                cp -a "$src_path" "$WEBOS_QT6_HOST_PREFIX/bin/$tool_name"
                return 0
            fi
            return 1
        }

        stage_tool qmake || true
        stage_tool moc || true
        stage_tool rcc || true
        stage_tool uic || true
        stage_tool qt-configure-module || true

        if [ -x "$WEBOS_QT6_HOST_PREFIX/bin/qmake" ] && [ ! -e "$WEBOS_QT6_HOST_PREFIX/bin/qmake6" ]; then
            ln -s qmake "$WEBOS_QT6_HOST_PREFIX/bin/qmake6"
        fi

        find "$WEBOS_QT6_BUILD_DIR" -maxdepth 4 -type f \( -name 'libQt6*.so*' -o -name 'libicu*.so*' -o -name 'libdouble-conversion*.so*' -o -name 'libzstd*.so*' -o -name 'libpcre2-*.so*' \) -print0 2>/dev/null | while IFS= read -r -d '' lib; do
            cp -a "$lib" "$WEBOS_QT6_HOST_PREFIX/lib/" || true
        done

        if [ -d "$WEBOS_QT6_BUILD_DIR/lib/cmake" ]; then
            mkdir -p "$WEBOS_QT6_HOST_PREFIX/lib/cmake"
            find "$WEBOS_QT6_BUILD_DIR/lib/cmake" -maxdepth 1 -mindepth 1 -type d -name 'Qt6*' -exec cp -a {} "$WEBOS_QT6_HOST_PREFIX/lib/cmake/" \; || true
        fi
    fi

    if [ ! -x "$WEBOS_QT6_HOST_PREFIX/bin/qmake6" ] && [ ! -x "$WEBOS_QT6_HOST_PREFIX/bin/qmake" ]; then
        echo "Failed to stage qmake/qmake6 into $WEBOS_QT6_HOST_PREFIX/bin"
        exit 1
    fi

    if [ -x "$WEBOS_QT6_HOST_PREFIX/bin/qt-configure-module" ] && [ -n "$WEBOS_QT6_HOST_MODULES" ]; then
        IFS=',' read -r -a qt_host_modules <<< "$WEBOS_QT6_HOST_MODULES"
        for module in "${qt_host_modules[@]}"; do
            module="$(echo "$module" | xargs)"
            [ -n "$module" ] || continue

            module_src="$(extract_qt6_module "$module")"
            module_build_dir="$WEBOS_QT6_BUILD_DIR-$module"

            "$WEBOS_QT6_HOST_PREFIX/bin/qt-configure-module" "$module_src" -- \
                -G "$WEBOS_QT6_CMAKE_GENERATOR" \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_INSTALL_PREFIX="$WEBOS_QT6_HOST_PREFIX" \
                -DQT_BUILD_EXAMPLES=OFF \
                -DQT_BUILD_TESTS=OFF \
                -B "$module_build_dir"

            cmake --build "$module_build_dir" --parallel "$JOBS"
            cmake --install "$module_build_dir"
        done
    fi

    echo ""
    echo "Qt6 host tools installed to: $WEBOS_QT6_HOST_PREFIX"
    echo "Use for ARM configure:" 
    echo "  WEBOS_QT6_HOST_TOOLS=$WEBOS_QT6_HOST_PREFIX/bin ./build-webos.sh configure"
}

write_qt6_arm_toolchain_file() {
    local out_file="$1"

    cat > "$out_file" <<EOF
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_SYSROOT "$SYSROOT")

set(CMAKE_C_COMPILER "${TARGET}-gcc")
set(CMAKE_CXX_COMPILER "${TARGET}-g++")
set(CMAKE_ASM_COMPILER "${TARGET}-gcc")

set(CMAKE_C_FLAGS_INIT "-march=armv7-a -mfloat-abi=softfp -mfpu=neon")
set(CMAKE_CXX_FLAGS_INIT "-march=armv7-a -mfloat-abi=softfp -mfpu=neon")

set(CMAKE_FIND_ROOT_PATH "$SYSROOT")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(PKG_CONFIG_SYSROOT_DIR "$SYSROOT")
set(PKG_CONFIG_LIBDIR "$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig")
EOF
}

build_qt6_target_arm() {
    if [ -z "$WEBOS_QT6_TARGET_PREFIX" ]; then
        WEBOS_QT6_TARGET_PREFIX="$HOME/qt6-webos/target/usr"
    fi

    if [ ! -x "$WEBOS_QT6_HOST_PREFIX/bin/qmake6" ] && [ ! -x "$WEBOS_QT6_HOST_PREFIX/bin/qmake" ]; then
        echo "Qt6 host tools missing in $WEBOS_QT6_HOST_PREFIX/bin; building host tools first..."
        build_qt6_host_tools
    fi

    if [ ! -x "$WEBOS_QT6_HOST_PREFIX/bin/qt-configure-module" ]; then
        echo "Missing qt-configure-module in $WEBOS_QT6_HOST_PREFIX/bin."
        echo "Run: $0 qt6-host-tools"
        exit 1
    fi

    if ! command -v cmake >/dev/null 2>&1; then
        echo "cmake was not found in PATH."
        exit 1
    fi

    if [ "$WEBOS_QT6_CMAKE_GENERATOR" = "Ninja" ] && ! command -v ninja >/dev/null 2>&1; then
        echo "ninja was not found in PATH (required for WEBOS_QT6_CMAKE_GENERATOR=Ninja)."
        exit 1
    fi

    download_qt6_sources
    mkdir -p "$WEBOS_QT6_TARGET_BUILD_DIR" "$WEBOS_QT6_TARGET_PREFIX"

    local toolchain_file
    if [ -n "$WEBOS_QT6_TARGET_TOOLCHAIN_FILE" ]; then
        toolchain_file="$WEBOS_QT6_TARGET_TOOLCHAIN_FILE"
    else
        toolchain_file="$WEBOS_QT6_TARGET_BUILD_DIR/qt6-arm-toolchain.cmake"
        write_qt6_arm_toolchain_file "$toolchain_file"
    fi

    if [ ! -f "$toolchain_file" ]; then
        echo "Qt6 ARM toolchain file not found: $toolchain_file"
        exit 1
    fi

    local qt_host_path
    qt_host_path="$WEBOS_QT6_HOST_PREFIX"
    if [ ! -f "$qt_host_path/lib/cmake/Qt6HostInfo/Qt6HostInfoConfig.cmake" ] && \
       [ -f "$WEBOS_QT6_BUILD_DIR/lib/cmake/Qt6HostInfo/Qt6HostInfoConfig.cmake" ]; then
        qt_host_path="$WEBOS_QT6_BUILD_DIR"
    fi

    export PATH="$WEBOS_QT6_HOST_PREFIX/bin:$PATH"
    local qt_host_qmake
    if [ -x "$WEBOS_QT6_HOST_PREFIX/bin/qmake6" ]; then
        qt_host_qmake="$WEBOS_QT6_HOST_PREFIX/bin/qmake6"
    else
        qt_host_qmake="$WEBOS_QT6_HOST_PREFIX/bin/qmake"
    fi
    export QMAKE6="$qt_host_qmake"
    [ -x "$WEBOS_QT6_HOST_PREFIX/bin/moc" ] && export MOC="$WEBOS_QT6_HOST_PREFIX/bin/moc"
    [ -x "$WEBOS_QT6_HOST_PREFIX/bin/rcc" ] && export RCC="$WEBOS_QT6_HOST_PREFIX/bin/rcc"
    [ -x "$WEBOS_QT6_HOST_PREFIX/bin/uic" ] && export UIC="$WEBOS_QT6_HOST_PREFIX/bin/uic"
    [ -x "$WEBOS_QT6_HOST_PREFIX/bin/qsb" ] && export QSB="$WEBOS_QT6_HOST_PREFIX/bin/qsb"
    [ -x "$WEBOS_QT6_HOST_PREFIX/bin/qmlcachegen" ] && export QMLCACHEGEN="$WEBOS_QT6_HOST_PREFIX/bin/qmlcachegen"

    local qtbase_src
    qtbase_src="$(extract_qt6_module qtbase)"
    apply_qt6_webos_compat_patches "$qtbase_src"
    local qtbase_build_dir="$WEBOS_QT6_TARGET_BUILD_DIR/qtbase"

    cmake -S "$qtbase_src" -B "$qtbase_build_dir" \
        -G "$WEBOS_QT6_CMAKE_GENERATOR" \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain_file" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$WEBOS_QT6_TARGET_PREFIX" \
        -DQT_HOST_PATH="$qt_host_path" \
        -DQT_BUILD_EXAMPLES=OFF \
        -DQT_BUILD_TESTS=OFF \
        -DFEATURE_gui=ON \
        -DFEATURE_widgets=ON

    cmake --build "$qtbase_build_dir" --parallel "$JOBS"
    cmake --install "$qtbase_build_dir"

    if [ -n "$WEBOS_QT6_TARGET_MODULES" ]; then
        IFS=',' read -r -a qt_target_modules <<< "$WEBOS_QT6_TARGET_MODULES"
        for module in "${qt_target_modules[@]}"; do
            module="$(echo "$module" | xargs)"
            [ -n "$module" ] || continue

            module_src="$(extract_qt6_module "$module")"
            module_build_dir="$WEBOS_QT6_TARGET_BUILD_DIR/$module"

            if [ "$module" = "qtdeclarative" ]; then
                apply_qt6_qtdeclarative_compat_patches "$module_src"
            fi

            local -a module_cmake_args
            module_cmake_args=(
                -G "$WEBOS_QT6_CMAKE_GENERATOR"
                -DCMAKE_TOOLCHAIN_FILE="$toolchain_file"
                -DCMAKE_BUILD_TYPE=Release
                -DCMAKE_INSTALL_PREFIX="$WEBOS_QT6_TARGET_PREFIX"
                -DCMAKE_PREFIX_PATH="$WEBOS_QT6_TARGET_PREFIX"
                -DQt6_DIR="$WEBOS_QT6_TARGET_PREFIX/lib/cmake/Qt6"
                -DQt6Core_DIR="$WEBOS_QT6_TARGET_PREFIX/lib/cmake/Qt6Core"
                -DQt6DBus_DIR="$WEBOS_QT6_TARGET_PREFIX/lib/cmake/Qt6DBus"
                -DQt6Gui_DIR="$WEBOS_QT6_TARGET_PREFIX/lib/cmake/Qt6Gui"
                -DQt6Network_DIR="$WEBOS_QT6_TARGET_PREFIX/lib/cmake/Qt6Network"
                -DQt6OpenGL_DIR="$WEBOS_QT6_TARGET_PREFIX/lib/cmake/Qt6OpenGL"
                -DQt6Widgets_DIR="$WEBOS_QT6_TARGET_PREFIX/lib/cmake/Qt6Widgets"
                -DQt6BuildInternals_DIR="$WEBOS_QT6_TARGET_PREFIX/lib/cmake/Qt6BuildInternals"
                -DQT_HOST_PATH="$qt_host_path"
                -DQT_BUILD_EXAMPLES=OFF
                -DQT_BUILD_TESTS=OFF
            )

            if [ "$module" = "qtdeclarative" ]; then
                module_cmake_args+=(
                    -DFEATURE_qml_network=OFF
                    -DINPUT_qml_network=no
                    -DINPUT_qml_ssl=no
                    -DQT_FEATURE_ssl=OFF
                    -DQt6ShaderTools_DIR="$WEBOS_QT6_TARGET_PREFIX/lib/cmake/Qt6ShaderTools"
                )

                if [ -d "$WEBOS_QT6_HOST_PREFIX/lib/cmake/Qt6QmlTools" ]; then
                    module_cmake_args+=( -DQt6QmlTools_DIR="$WEBOS_QT6_HOST_PREFIX/lib/cmake/Qt6QmlTools" )
                fi
                if [ -d "$WEBOS_QT6_HOST_PREFIX/lib/cmake/Qt6ShaderToolsTools" ]; then
                    module_cmake_args+=( -DQt6ShaderToolsTools_DIR="$WEBOS_QT6_HOST_PREFIX/lib/cmake/Qt6ShaderToolsTools" )
                fi
                if [ -d "$WEBOS_QT6_HOST_PREFIX/lib/cmake" ]; then
                    module_cmake_args+=( -DQT_HOST_PATH_CMAKE_DIR="$WEBOS_QT6_HOST_PREFIX/lib/cmake" )
                fi
            fi

            module_cmake_args+=( -B "$module_build_dir" )

            "$WEBOS_QT6_HOST_PREFIX/bin/qt-configure-module" "$module_src" -- "${module_cmake_args[@]}"

            cmake --build "$module_build_dir" --parallel "$JOBS"
            cmake --install "$module_build_dir"
        done
    fi

    echo ""
    echo "Qt6 ARM target runtime installed to: $WEBOS_QT6_TARGET_PREFIX"
    echo "Use with VLC build/package:"
    echo "  WEBOS_QT6_HOST_TOOLS=$WEBOS_QT6_HOST_PREFIX/bin WEBOS_QT6_TARGET_PREFIX=$WEBOS_QT6_TARGET_PREFIX ./build-webos.sh configure"
    echo "  WEBOS_QT_RUNTIME_DIR=$WEBOS_QT6_TARGET_PREFIX extras/package/webos/package.sh"
}

case "$MODE" in
    qt6|qt6-host-tools|qt6-target-arm|sdk|deps|configure|build|install|all)
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

if [ "$MODE" = "qt6" ]; then
    download_qt6_sources
    exit 0
fi

if [ "$MODE" = "qt6-host-tools" ]; then
    build_qt6_host_tools
    exit 0
fi

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
            "$la" || true

        if [ -n "${WEBOS_TOOLCHAIN:-}" ]; then
            sed -i \
                -e "s|/__w/buildroot-nc4/buildroot-nc4/output/host/${TARGET}/lib|${WEBOS_TOOLCHAIN}/${TARGET}/lib|g" \
                -e "s|${WEBOS_TOOLCHAIN}/lib|${WEBOS_TOOLCHAIN}/${TARGET}/lib|g" \
                "$la" || true
        fi

    done < <(find "${SYSROOT}/usr/lib" "${DEPS_PREFIX}/lib" -maxdepth 1 -type f -name '*.la' -print0 2>/dev/null)
}

sanitize_makefile_paths() {
    local makefile="$1"
    [ -f "$makefile" ] || return 0
    sed -E -i \
        -e "s|-I/usr/include/libpng16|-I${SYSROOT}/usr/include/libpng16|g" \
        -e "s|-I/usr/include/freetype2|-I${SYSROOT}/usr/include/freetype2|g" \
        -e "s|-I/usr/include/harfbuzz|-I${SYSROOT}/usr/include/harfbuzz|g" \
        -e "s|-I/usr/include/libxml2|-I${SYSROOT}/usr/include/libxml2|g" \
        -e "s|-I/usr/lib/(libffi[^[:space:]]*/include)|-I${SYSROOT}/usr/lib/\1|g" \
        -e "s|-L/usr/lib\\>|-L${SYSROOT}/usr/lib|g" \
        "$makefile" || true
}

append_include_flag_if_exists() {
    local dir="$1"
    [ -d "$dir" ] || return 0

    case " ${WEBOS_EXTRA_INCLUDE_FLAGS:-} " in
        *" -I${dir} "*) return 0 ;;
    esac

    WEBOS_EXTRA_INCLUDE_FLAGS="${WEBOS_EXTRA_INCLUDE_FLAGS:-} -I${dir}"
}

prepare_webos_include_flags() {
    WEBOS_EXTRA_INCLUDE_FLAGS=""

    append_include_flag_if_exists "${DEPS_PREFIX}/include/harfbuzz"
    append_include_flag_if_exists "${SYSROOT}/usr/include/harfbuzz"
    append_include_flag_if_exists "${SYSROOT}/usr/lib/libffi/include"
    append_include_flag_if_exists "${DEPS_PREFIX}/lib/libffi/include"

    local dir
    for dir in "${SYSROOT}"/usr/lib/libffi*/include "${DEPS_PREFIX}"/lib/libffi*/include; do
        append_include_flag_if_exists "$dir"
    done
}

have_cross_pkg() {
    local pkg="$1"
    PKG_CONFIG_SYSROOT_DIR="${PKG_CONFIG_SYSROOT_DIR:-}" \
    PKG_CONFIG_LIBDIR="${PKG_CONFIG_LIBDIR:-}" \
    PKG_CONFIG_PATH="${PKG_CONFIG_PATH:-}" \
    pkg-config --exists "$pkg" >/dev/null 2>&1
}

have_header_in_cross_paths() {
    local rel="$1"
    [ -f "${DEPS_PREFIX}/include/${rel}" ] || [ -f "${SYSROOT}/usr/include/${rel}" ]
}

resolve_qmake_binary() {
    local tool_dir="$1"

    if [ -x "$tool_dir/qmake6" ]; then
        printf '%s\n' "$tool_dir/qmake6"
        return 0
    fi

    if [ -x "$tool_dir/qmake" ]; then
        printf '%s\n' "$tool_dir/qmake"
        return 0
    fi

    return 1
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

if [ -n "$WEBOS_QT6_ROOT" ]; then
    if [ -z "$WEBOS_QT6_HOST_TOOLS" ] && [ -d "$WEBOS_QT6_ROOT/host/bin" ]; then
        WEBOS_QT6_HOST_TOOLS="$WEBOS_QT6_ROOT/host/bin"
    fi

    if [ -z "$WEBOS_QT6_TARGET_PREFIX" ]; then
        if [ -d "$WEBOS_QT6_ROOT/target/usr" ]; then
            WEBOS_QT6_TARGET_PREFIX="$WEBOS_QT6_ROOT/target/usr"
        elif [ -d "$WEBOS_QT6_ROOT/target" ]; then
            WEBOS_QT6_TARGET_PREFIX="$WEBOS_QT6_ROOT/target"
        fi
    fi
fi

if [ -n "${WEBOS_TOOLCHAIN:-}" ] && [ -x "${WEBOS_TOOLCHAIN}/bin/qmake" ]; then
    export QMAKE="${WEBOS_TOOLCHAIN}/bin/qmake"
fi

if [ "$QT_ENABLED" = "1" ]; then
    qt6_tools_dir=""
    qt_qmake_bin=""

    if [ -n "$WEBOS_QT6_HOST_TOOLS" ] && qt_qmake_bin="$(resolve_qmake_binary "$WEBOS_QT6_HOST_TOOLS")"; then
        qt6_tools_dir="$WEBOS_QT6_HOST_TOOLS"
    elif [ -n "${WEBOS_TOOLCHAIN:-}" ] && qt_qmake_bin="$(resolve_qmake_binary "${WEBOS_TOOLCHAIN}/bin")"; then
        qt6_tools_dir="${WEBOS_TOOLCHAIN}/bin"
    fi

    if [ -z "$qt6_tools_dir" ] && [ "${WEBOS_QT6_AUTO_DOWNLOAD:-1}" = "1" ]; then
        echo "Qt6 host tools not found. Downloading Qt6 sources first..."
        download_qt6_sources
    fi

    if [ -z "$qt6_tools_dir" ]; then
        echo "Qt is enabled, but the cross toolchain does not provide qmake/qmake6."
        echo "Current toolchain: ${WEBOS_TOOLCHAIN:-<unset>}"
        echo "Qt6 sources directory: ${WEBOS_QT6_SRC_DIR}"
        echo "Set WEBOS_QT6_HOST_TOOLS to external Qt6 host tools, or install an ARM Qt6-capable toolchain."
        echo "Example: WEBOS_QT6_HOST_TOOLS=/opt/qt6-webos/host/bin ./build-webos.sh configure"
        echo "Otherwise disable Qt explicitly:"
        echo "  WEBOS_CONFIGURE_EXTRA_FLAGS=\"--disable-qt\" ./build-webos.sh configure"
        exit 1
    fi

    if [ -n "$WEBOS_QT6_TARGET_PREFIX" ]; then
        qt_target_conf="$WEBOS_QT6_TARGET_PREFIX/bin/target_qt.conf"
        if [ -f "$qt_target_conf" ]; then
            sed -i \
                -e 's/^SysrootifyPrefix=.*/SysrootifyPrefix=false/' \
                -e "s|^Sysroot=.*|Sysroot=${SYSROOT}|" \
                "$qt_target_conf"
        fi
    fi

    export PATH="$qt6_tools_dir:$PATH"
    if [ -n "$WEBOS_QT6_TARGET_PREFIX" ] && [ -x "$WEBOS_QT6_TARGET_PREFIX/bin/qmake6" ]; then
        export QMAKE6="$WEBOS_QT6_TARGET_PREFIX/bin/qmake6"
    elif [ -n "$WEBOS_QT6_TARGET_PREFIX" ] && [ -x "$WEBOS_QT6_TARGET_PREFIX/bin/qmake" ]; then
        export QMAKE6="$WEBOS_QT6_TARGET_PREFIX/bin/qmake"
    else
        export QMAKE6="$qt_qmake_bin"
    fi
    export QMAKE="$QMAKE6"
    [ -x "$qt6_tools_dir/moc" ] && export MOC="$qt6_tools_dir/moc"
    [ -x "$qt6_tools_dir/rcc" ] && export RCC="$qt6_tools_dir/rcc"
    [ -x "$qt6_tools_dir/uic" ] && export UIC="$qt6_tools_dir/uic"
    [ -x "$qt6_tools_dir/qsb" ] && export QSB="$qt6_tools_dir/qsb"
    [ -x "$qt6_tools_dir/qmlcachegen" ] && export QMLCACHEGEN="$qt6_tools_dir/qmlcachegen"

    qtbase_src_dir="$WEBOS_QT6_SRC_DIR/qtbase-everywhere-src-$WEBOS_QT6_VERSION"
    if [ -d "$qtbase_src_dir/mkspecs" ]; then
        qmake_host_data="$("$QMAKE6" -query QT_HOST_DATA 2>/dev/null || true)"
        if [ -n "$qmake_host_data" ] && [ ! -d "$qmake_host_data/mkspecs/linux-g++" ]; then
            mkdir -p "$qmake_host_data"
            if [ ! -e "$qmake_host_data/mkspecs" ]; then
                ln -s "$qtbase_src_dir/mkspecs" "$qmake_host_data/mkspecs" || true
            else
                cp -a "$qtbase_src_dir/mkspecs/." "$qmake_host_data/mkspecs/" 2>/dev/null || true
            fi
        fi

        export QMAKEPATH="$qtbase_src_dir/mkspecs${QMAKEPATH:+:$QMAKEPATH}"
        export QMAKESPEC="${QMAKESPEC:-linux-g++}"
    fi
fi

export PKG_CONFIG_SYSROOT_DIR=""
export PKG_CONFIG_LIBDIR="${DEPS_PREFIX}/lib/pkgconfig:${DEPS_PREFIX}/share/pkgconfig:${SYSROOT}/usr/lib/pkgconfig:${SYSROOT}/usr/share/pkgconfig"
export PKG_CONFIG_PATH="${DEPS_PREFIX}/lib/pkgconfig:${DEPS_PREFIX}/share/pkgconfig"
prepare_webos_include_flags

if [ "$QT_ENABLED" = "1" ] && [ -n "$WEBOS_QT6_TARGET_PREFIX" ]; then
    if [ -d "$WEBOS_QT6_TARGET_PREFIX/lib/pkgconfig" ]; then
        export PKG_CONFIG_LIBDIR="$WEBOS_QT6_TARGET_PREFIX/lib/pkgconfig:$PKG_CONFIG_LIBDIR"
        export PKG_CONFIG_PATH="$WEBOS_QT6_TARGET_PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH"
    fi
    if [ -d "$WEBOS_QT6_TARGET_PREFIX/share/pkgconfig" ]; then
        export PKG_CONFIG_LIBDIR="$WEBOS_QT6_TARGET_PREFIX/share/pkgconfig:$PKG_CONFIG_LIBDIR"
        export PKG_CONFIG_PATH="$WEBOS_QT6_TARGET_PREFIX/share/pkgconfig:$PKG_CONFIG_PATH"
    fi
fi
export CPPFLAGS="${CPPFLAGS:-} -I${DEPS_PREFIX}/include -I${SYSROOT}/usr/include${WEBOS_EXTRA_INCLUDE_FLAGS}"
export CFLAGS="${CFLAGS:-} -march=armv7-a -mfloat-abi=softfp -mfpu=neon -I${DEPS_PREFIX}/include${WEBOS_EXTRA_INCLUDE_FLAGS}"
export CXXFLAGS="${CXXFLAGS:-} -march=armv7-a -mfloat-abi=softfp -mfpu=neon -I${DEPS_PREFIX}/include${WEBOS_EXTRA_INCLUDE_FLAGS}"
export LDFLAGS="${LDFLAGS:-} -L${DEPS_PREFIX}/lib -L${SYSROOT}/usr/lib"

normalize_sysroot_libtool_archives

if ! command -v "${TARGET}-gcc" >/dev/null 2>&1; then
    echo "${TARGET}-gcc was not found in PATH."
    echo "Check WEBOS_TOOLCHAIN: ${WEBOS_TOOLCHAIN}"
    exit 1
fi

if [ "$MODE" = "qt6-target-arm" ]; then
    build_qt6_target_arm
    exit 0
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

        if ! have_cross_pkg libdrm || ! have_header_in_cross_paths drm/drm_fourcc.h; then
            echo "libdrm not available in cross sysroot; disabling DRM display plugin for this build."
            sed -E -i 's/^(am__append_[0-9]+ = libdrm_display_plugin\.la)$/# \1/' modules/Makefile || true
        fi

        if ! have_cross_pkg egl || ! have_header_in_cross_paths EGL/egl.h; then
            echo "EGL headers/libs not available in cross sysroot; disabling generic EGL display plugin for this build."
            sed -E -i 's/^(am__append_[0-9]+ = libegl_display_generic_plugin\.la)$/# \1/' modules/Makefile || true
        fi

        if ! have_cross_pkg gstreamer-app-1.0 || ! have_cross_pkg gstreamer-video-1.0 || ! have_cross_pkg gstreamer-allocators-1.0 ||
           ! { have_header_in_cross_paths gst/gst.h || have_header_in_cross_paths gstreamer-1.0/gst/gst.h; }; then
            echo "GStreamer development files not available in cross sysroot; disabling gstdecode/gst_mem plugins for this build."
            sed -E -i 's/^(am__append_[0-9]+ = libgstdecode_plugin\.la)$/# \1/' modules/Makefile || true
            sed -E -i 's/^(am__append_[0-9]+ = libgst_mem_plugin\.la)$/# \1/' modules/Makefile || true
        fi

        if [ "$WEBOS_DISABLE_FREETYPE_ON_MISSING_HB" = "1" ] &&
           [ ! -f "${DEPS_PREFIX}/include/harfbuzz/hb-ft.h" ] &&
           [ ! -f "${SYSROOT}/usr/include/harfbuzz/hb-ft.h" ] &&
           [ ! -f "${DEPS_PREFIX}/include/hb-ft.h" ] &&
           [ ! -f "${SYSROOT}/usr/include/hb-ft.h" ]; then
            echo "Harfbuzz hb-ft.h not found in deps/sysroot; disabling freetype text renderer plugin for this build."
            sed -E -i 's/^(am__append_[0-9]+ = libfreetype_plugin\.la)$/# \1/' modules/Makefile || true
        fi

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

    # Post-build fixups required for the Qt GUI plugin on webOS:
    #
    # 1. patch-qt-makefile.py  — creates libtool .la stubs for all
    #    libqml_module_*.a convenience archives and adds them to
    #    libqt_plugin_la_LIBADD so the linker wraps them in --whole-archive.
    #    Without this, Qt's constructor-based QML module registration is
    #    silently dropped as dead code.
    #
    # 2. embed-qml-sources.py  — embeds all 241 .qml source files as Qt
    #    resources inside the plugin.  The *_qmlassets.cpp generated by the
    #    build system only embeds the qmldir descriptor; Qt needs the source
    #    files present in the resource system before it will invoke the
    #    pre-compiled bytecode cache hook from *_qmlcache_loader.cpp.
    #
    # Both scripts then force a relink of libqt_plugin.so by removing the
    # stale .so/.la and re-running make on that single target.
    _QT_PLUGIN_BUILD_DIR="${BUILD_DIR}/modules/gui/qt"
    if [ -f "${_QT_PLUGIN_BUILD_DIR}/Makefile" ] && [ -x "${WEBOS_TOOLCHAIN}/bin/${TARGET}-g++" ]; then
        echo "=== Applying Qt plugin post-build fixups ==="
        python3 "${SRC_DIR}/extras/buildsystem/patch-qt-makefile.py" \
            --build-dir "${_QT_PLUGIN_BUILD_DIR}" \
            --src-dir   "${SRC_DIR}"
        python3 "${SRC_DIR}/extras/buildsystem/embed-qml-sources.py" \
            --build-dir "${_QT_PLUGIN_BUILD_DIR}" \
            --src-dir   "${SRC_DIR}" \
            --toolchain "${WEBOS_TOOLCHAIN}" \
            --target    "${TARGET}"
        rm -f "${_QT_PLUGIN_BUILD_DIR}/.libs/libqt_plugin.so" \
              "${_QT_PLUGIN_BUILD_DIR}/libqt_plugin.la"
        make -C "${_QT_PLUGIN_BUILD_DIR}" libqt_plugin.la
        echo "=== Qt plugin fixup complete ==="
    fi
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
