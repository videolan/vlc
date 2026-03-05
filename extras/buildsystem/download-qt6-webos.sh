#!/usr/bin/env bash

set -euo pipefail

QT_VERSION="${QT_VERSION:-6.8.3}"
QT_SERIES="${QT_VERSION%.*}"
OUT_DIR="${OUT_DIR:-$HOME/qt6-webos-src-$QT_VERSION}"
BASE_URL="${BASE_URL:-https://download.qt.io/archive/qt/${QT_SERIES}/${QT_VERSION}/submodules}"

# Minimum set for VLC Qt6/QML UI stack on embedded targets.
MODULES=(
    qtbase
    qtdeclarative
    qtshadertools
    qttools
    qtwayland
)

download_file() {
    local url="$1"
    local out="$2"

    if command -v curl >/dev/null 2>&1; then
        curl -fL --retry 3 --retry-delay 2 "$url" -o "$out"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$out" "$url"
    else
        echo "Neither curl nor wget is available."
        exit 1
    fi
}

mkdir -p "$OUT_DIR"

for module in "${MODULES[@]}"; do
    archive="${module}-everywhere-src-${QT_VERSION}.tar.xz"
    url="${BASE_URL}/${archive}"
    out="${OUT_DIR}/${archive}"

    if [ -f "$out" ]; then
        echo "Using existing: $out"
        continue
    fi

    echo "Downloading ${archive}"
    download_file "$url" "$out"
done

echo ""
echo "Downloaded Qt ${QT_VERSION} sources to: $OUT_DIR"
echo "Next step: cross-build host tools + ARM target Qt6 against your webOS sysroot."
