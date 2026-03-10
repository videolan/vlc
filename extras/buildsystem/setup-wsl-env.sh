#!/usr/bin/env bash
# setup-wsl-env.sh — Bootstrap a WSL Ubuntu 24.04 build environment for VLC webOS
#
# Usage:
#   ./extras/buildsystem/setup-wsl-env.sh [all|packages|sdk|qt6-host|qt6-arm|ares]
#
# Phases (can be combined):
#   packages   — apt-get install all host build deps
#   sdk        — download/extract the webOS ARM cross-toolchain (buildroot-nc4)
#   qt6-host   — build Qt6 host tools (qmake6/moc/rcc/uic/qsb) from source
#   qt6-arm    — cross-compile Qt6 ARM target libraries (qtbase + QML stack)
#   ares       — install webOS CLI tools (ares-package / ares-install / ares-launch)
#   all        — run all phases above (default)
#
# Key environment overrides (inherit from parent or set on command line):
#   WEBOS_TOOLCHAIN     — path to extracted SDK (auto-detected or auto-downloaded)
#   WEBOS_QT6_VERSION   — Qt6 version (default: 6.8.3)
#   WEBOS_QT6_HOST_PREFIX  — where Qt6 host tools are installed (default: ~/qt6-webos/host)
#   WEBOS_QT6_TARGET_PREFIX — where Qt6 ARM libs are installed (default: ~/qt6-webos/target/usr)
#   WEBOS_SDK_URL       — custom SDK tarball URL
#   WEBOS_SDK_ARCHIVE   — local SDK tarball path (skip download)
#   JOBS                — parallel make jobs (default: nproc)

set -euo pipefail

# ---------------------------------------------------------------------------
# Locate repository root (the script lives in extras/buildsystem/)
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

PHASE="${1:-all}"

JOBS="${JOBS:-$(nproc)}"
WEBOS_QT6_VERSION="${WEBOS_QT6_VERSION:-6.8.3}"
WEBOS_QT6_HOST_PREFIX="${WEBOS_QT6_HOST_PREFIX:-$HOME/qt6-webos/host}"
WEBOS_QT6_TARGET_PREFIX="${WEBOS_QT6_TARGET_PREFIX:-$HOME/qt6-webos/target/usr}"
BUILD_WEBOS="$REPO_ROOT/build-webos.sh"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
step() { printf '\n\033[1;36m=== %s ===\033[0m\n' "$*"; }
ok()   { printf '\033[1;32m  OK: %s\033[0m\n' "$*"; }
warn() { printf '\033[1;33m  WARN: %s\033[0m\n' "$*"; }

require_root_or_sudo() {
    if [ "$(id -u)" -eq 0 ]; then
        return 0  # already root
    fi
    # Test sudo access (allow password prompt when there is a TTY)
    if ! sudo true 2>/dev/null; then
        echo "ERROR: This script needs sudo for apt-get."
        echo "  Either run as root:  wsl -d Ubuntu-24.04 -u root $0 packages"
        echo "  Or ensure the current user has passwordless sudo."
        exit 1
    fi
}

run_as_sudo() {
    if [ "$(id -u)" -eq 0 ]; then
        "$@"
    else
        sudo "$@"
    fi
}

# ---------------------------------------------------------------------------
# Phase: packages
# ---------------------------------------------------------------------------
install_packages() {
    step "Installing host build dependencies (apt)"
    require_root_or_sudo

    run_as_sudo apt-get update -y

    # Core build chain
    run_as_sudo apt-get install -y \
        build-essential \
        gcc \
        g++ \
        make \
        cmake \
        ninja-build \
        meson \
        pkg-config \
        libtool \
        autoconf \
        automake \
        autopoint \
        gettext \
        m4 \
        flex \
        bison \
        gperf \
        texinfo \
        nasm \
        yasm \
        perl \
        python3 \
        python3-pip \
        python3-setuptools \
        patch \
        diffutils \
        file \
        rsync \
        unzip \
        xz-utils \
        tar \
        sed \
        git \
        curl \
        wget \
        libssl-dev \
        zlib1g-dev \
        liblzma-dev \
        libbz2-dev \
        libffi-dev \
        libedit-dev

    # Needed by some contribs (lua, protobuf, etc.)
    run_as_sudo apt-get install -y \
        lua5.2 \
        liblua5.2-dev \
        protobuf-compiler \
        libprotobuf-dev || true

    # Qt6 host build requirements
    run_as_sudo apt-get install -y \
        libgl-dev \
        libgles-dev \
        libegl-dev \
        libopengl-dev \
        libx11-dev \
        libxext-dev \
        libxcb1-dev \
        libxcb-glx0-dev \
        libxcb-keysyms1-dev \
        libxcb-image0-dev \
        libxcb-shm0-dev \
        libxcb-icccm4-dev \
        libxcb-sync-dev \
        libxcb-xfixes0-dev \
        libxcb-randr0-dev \
        libxcb-util-dev \
        libxcb-xinerama0-dev \
        libxcb-xkb-dev \
        libxkbcommon-dev \
        libxkbcommon-x11-dev \
        libfontconfig-dev \
        libfreetype-dev \
        libglib2.0-dev \
        libdbus-1-dev \
        libwayland-dev \
        wayland-protocols \
        libinput-dev \
        libmtdev-dev \
        libudev-dev \
        libgbm-dev \
        libdrm-dev || true

    # Node.js for ares-cli (use NodeSource LTS if not present)
    if ! command -v node >/dev/null 2>&1; then
        step "Installing Node.js LTS (via NodeSource)"
        run_as_sudo apt-get install -y ca-certificates gnupg
        curl -fsSL https://deb.nodesource.com/setup_lts.x | run_as_sudo bash -
        run_as_sudo apt-get install -y nodejs
    fi

    ok "Host packages installed."
}

# ---------------------------------------------------------------------------
# Phase: sdk  (webOS ARM cross-toolchain)
# ---------------------------------------------------------------------------
install_sdk() {
    step "Installing webOS ARM cross-toolchain (buildroot-nc4 / openlgtv)"

    export JOBS="$JOBS"

    chmod +x "$BUILD_WEBOS"
    "$BUILD_WEBOS" sdk

    ok "webOS SDK ready."
}

# ---------------------------------------------------------------------------
# Phase: qt6-host  (build Qt6 x86_64 host tools from source)
# ---------------------------------------------------------------------------
build_qt6_host() {
    step "Building Qt6 host tools (version $WEBOS_QT6_VERSION)"

    export JOBS="$JOBS"
    export WEBOS_QT6_VERSION="$WEBOS_QT6_VERSION"
    export WEBOS_QT6_HOST_PREFIX="$WEBOS_QT6_HOST_PREFIX"

    # Ensure cmake + ninja are present
    if ! command -v cmake >/dev/null 2>&1; then
        echo "cmake not found. Run the 'packages' phase first."
        exit 1
    fi
    if ! command -v ninja >/dev/null 2>&1; then
        echo "ninja not found. Run the 'packages' phase first."
        exit 1
    fi

    chmod +x "$BUILD_WEBOS"
    "$BUILD_WEBOS" qt6-host-tools

    ok "Qt6 host tools installed at $WEBOS_QT6_HOST_PREFIX"
}

# ---------------------------------------------------------------------------
# Phase: qt6-arm  (cross-compile Qt6 ARM target libraries)
# ---------------------------------------------------------------------------
build_qt6_arm() {
    step "Cross-compiling Qt6 ARM target (version $WEBOS_QT6_VERSION)"

    export JOBS="$JOBS"
    export WEBOS_QT6_VERSION="$WEBOS_QT6_VERSION"
    export WEBOS_QT6_HOST_PREFIX="$WEBOS_QT6_HOST_PREFIX"
    export WEBOS_QT6_TARGET_PREFIX="$WEBOS_QT6_TARGET_PREFIX"
    # build-webos.sh checks WEBOS_QT6_HOST_TOOLS (not HOST_PREFIX) before
    # dispatching qt6-target-arm, so we must export it explicitly.
    export WEBOS_QT6_HOST_TOOLS="$WEBOS_QT6_HOST_PREFIX/bin"

    if [ ! -x "$WEBOS_QT6_HOST_PREFIX/bin/qmake6" ] && [ ! -x "$WEBOS_QT6_HOST_PREFIX/bin/qmake" ]; then
        warn "Qt6 host tools not found at $WEBOS_QT6_HOST_PREFIX. Running qt6-host phase first..."
        build_qt6_host
    fi

    chmod +x "$BUILD_WEBOS"
    "$BUILD_WEBOS" qt6-target-arm

    ok "Qt6 ARM target installed at $WEBOS_QT6_TARGET_PREFIX"
}

# ---------------------------------------------------------------------------
# Phase: ares  (webOS CLI packaging tools)
# ---------------------------------------------------------------------------
install_ares() {
    step "Installing webOS ares-cli tools (ares-package / ares-install / ares-launch)"

    if ! command -v node >/dev/null 2>&1; then
        echo "Node.js not found. Run the 'packages' phase first."
        exit 1
    fi

    ARES_PKG="@webosose/ares-cli"
    if command -v ares-package >/dev/null 2>&1; then
        ok "ares-cli already installed ($(ares-package --version 2>/dev/null || true))"
        return 0
    fi

    # Prefer local user install to avoid sudo for npm
    npm install -g "$ARES_PKG" 2>/dev/null || \
        sudo npm install -g "$ARES_PKG"

    if command -v ares-package >/dev/null 2>&1; then
        ok "ares-cli installed successfully."
    else
        warn "ares-package not in PATH after install. You may need to add npm global bin to PATH:"
        warn "  export PATH=\"\$(npm bin -g 2>/dev/null || npm root -g)/../bin:\$PATH\""
    fi
}

# ---------------------------------------------------------------------------
# Summary banner
# ---------------------------------------------------------------------------
print_summary() {
    step "Environment summary"

    echo ""
    echo "  REPO_ROOT             : $REPO_ROOT"
    echo "  JOBS                  : $JOBS"
    echo "  WEBOS_QT6_VERSION     : $WEBOS_QT6_VERSION"
    echo "  WEBOS_QT6_HOST_PREFIX : $WEBOS_QT6_HOST_PREFIX"
    echo "  WEBOS_QT6_TARGET_PREFIX: $WEBOS_QT6_TARGET_PREFIX"
    echo ""

    # Toolchain
    TOOLCHAIN_OK=0
    for candidate in \
        "$HOME/vlc-webos-build/sdk/arm-webos-linux-gnueabi_sdk-buildroot" \
        "$HOME/vlc-webos-build/sdk/arm-webos-linux-gnueabi_sdk-buildroot-x86_64" \
        "$HOME/kodi-dev/arm-webos-linux-gnueabi_sdk-buildroot"; do
        if [ -f "$candidate/arm-webos-linux-gnueabi/sysroot/usr/lib/crt1.o" ]; then
            echo "  webOS toolchain       : $candidate  [FOUND]"
            TOOLCHAIN_OK=1
            break
        fi
    done
    [ "$TOOLCHAIN_OK" -eq 0 ] && warn "webOS toolchain        : NOT FOUND"

    # Qt6 host tools
    if [ -x "$WEBOS_QT6_HOST_PREFIX/bin/qmake6" ] || [ -x "$WEBOS_QT6_HOST_PREFIX/bin/qmake" ]; then
        echo "  Qt6 host tools        : $WEBOS_QT6_HOST_PREFIX/bin  [FOUND]"
    else
        warn "Qt6 host tools         : NOT FOUND at $WEBOS_QT6_HOST_PREFIX"
    fi

    # Qt6 ARM target
    if [ -d "$WEBOS_QT6_TARGET_PREFIX/lib/cmake/Qt6" ]; then
        echo "  Qt6 ARM target        : $WEBOS_QT6_TARGET_PREFIX  [FOUND]"
    else
        warn "Qt6 ARM target         : NOT FOUND at $WEBOS_QT6_TARGET_PREFIX"
    fi

    # ares-cli
    if command -v ares-package >/dev/null 2>&1; then
        echo "  ares-package          : $(command -v ares-package)  [FOUND]"
    else
        warn "ares-package           : NOT in PATH"
    fi

    echo ""
    echo "Next step — full build + IPK:"
    echo ""
    echo "  cd $REPO_ROOT"
    echo "  WEBOS_QT6_HOST_TOOLS=$WEBOS_QT6_HOST_PREFIX/bin \\"
    echo "  WEBOS_QT6_TARGET_PREFIX=$WEBOS_QT6_TARGET_PREFIX \\"
    echo "  DEPLOY_DIR=$REPO_ROOT/vlc-webos-deploy \\"
    echo "  PREFIX=/ \\"
    echo "  ./build-webos.sh all"
    echo ""
    echo "  Then package:"
    echo "  WEBOS_QT_RUNTIME_DIR=$WEBOS_QT6_TARGET_PREFIX \\"
    echo "  ./extras/package/webos/package.sh"
    echo ""
}

# ---------------------------------------------------------------------------
# Dispatch
# ---------------------------------------------------------------------------
case "$PHASE" in
    packages)
        install_packages
        ;;
    sdk)
        install_sdk
        ;;
    qt6-host)
        build_qt6_host
        ;;
    qt6-arm)
        build_qt6_arm
        ;;
    ares)
        install_ares
        ;;
    all)
        install_packages
        install_sdk
        build_qt6_host
        build_qt6_arm
        install_ares
        print_summary
        ;;
    summary)
        print_summary
        ;;
    -h|--help|help)
        sed -n '2,20p' "${BASH_SOURCE[0]}"
        exit 0
        ;;
    *)
        echo "Unknown phase: $PHASE"
        echo "Usage: $0 [all|packages|sdk|qt6-host|qt6-arm|ares|summary]"
        exit 1
        ;;
esac
