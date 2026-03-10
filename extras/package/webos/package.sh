#!/bin/sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
. "$SCRIPT_DIR/env.build.sh"

APP_ID="${WEBOS_APP_ID:-org.videolan.vlc.webos}"
APP_VERSION="${WEBOS_APP_VERSION:-1.0.0}"
TARGET_ARCH="${WEBOS_TARGET_ARCH:-arm}"
if [ -n "${WEBOS_DEPLOY_DIR:-}" ]; then
    DEPLOY_DIR="$WEBOS_DEPLOY_DIR"
else
    DEPLOY_DIR="$TOP_DIR/vlc-webos-deploy"
fi
OUT_DIR="${WEBOS_OUTPUT_DIR:-$TOP_DIR/webos-package}"
PKG_WORK_DIR="${WEBOS_PACKAGE_DIR:-$TOP_DIR/webos-package/stage}"
WEBOS_QT_RUNTIME_DIR="${WEBOS_QT_RUNTIME_DIR:-}"
WEBOS_PACKAGE_PROFILE="${WEBOS_PACKAGE_PROFILE:-full}"

# Auto-detect the webOS ARM toolchain for runtime library bundling.
# Mirrors build-webos.sh detection: env var first, then default SDK_DOWNLOAD_DIR.
if [ -z "${WEBOS_TOOLCHAIN:-}" ]; then
    _BUILD_DIR="${BUILD_DIR:-$HOME/vlc-webos-build}"
    for _tc_candidate in \
        "$_BUILD_DIR/sdk/arm-webos-linux-gnueabi_sdk-buildroot" \
        "$_BUILD_DIR/sdk/arm-webos-linux-gnueabi_sdk-buildroot-x86_64" \
        "$HOME/kodi-dev/arm-webos-linux-gnueabi_sdk-buildroot"; do
        if [ -d "$_tc_candidate" ]; then
            WEBOS_TOOLCHAIN="$_tc_candidate"
            break
        fi
    done
fi

# Auto-detect contrib deps prefix (~/vlc-webos-deps by default).
if [ -z "${WEBOS_DEPS_PREFIX:-}" ]; then
    WEBOS_DEPS_PREFIX="${DEPS_PREFIX:-$HOME/vlc-webos-deps}"
fi

if [ ! -d "$DEPLOY_DIR" ]; then
    echo "Missing deploy directory: $DEPLOY_DIR"
    echo "Run: WEBOS_TOOLCHAIN=... PREFIX=/ DEPLOY_DIR=$DEPLOY_DIR extras/package/webos/build-webos.sh all"
    exit 1
fi

resolve_vlc_root() {
    local base="$1"

    if [ -d "$base/lib" ] || [ -d "$base/bin" ]; then
        printf '%s\n' "$base"
        return 0
    fi

    local marker
    marker="$(find "$base" -type f -name 'libvlc.so*' -path '*/lib/*' 2>/dev/null | head -n 1)"
    if [ -n "$marker" ]; then
        dirname "$(dirname "$marker")"
        return 0
    fi

    return 1
}

if VLC_ROOT="$(resolve_vlc_root "$DEPLOY_DIR")"; then
    :
else
    echo "Could not resolve VLC root under: $DEPLOY_DIR"
    exit 1
fi

VLC_PLUGIN_SRC=""
if [ -d "$VLC_ROOT/plugins" ]; then
    VLC_PLUGIN_SRC="$VLC_ROOT/plugins"
elif [ -d "$VLC_ROOT/lib/vlc/plugins" ]; then
    VLC_PLUGIN_SRC="$VLC_ROOT/lib/vlc/plugins"
else
    echo "No VLC plugins directory found under $VLC_ROOT"
    exit 1
fi

echo "=== webOS IPK packaging (in-repo) ==="
echo "APP_ID:        $APP_ID"
echo "APP_VERSION:   $APP_VERSION"
echo "TARGET_ARCH:   $TARGET_ARCH"
echo "DEPLOY_DIR:    $DEPLOY_DIR"
echo "VLC_ROOT:      $VLC_ROOT"
echo "PLUGIN_SRC:    $VLC_PLUGIN_SRC"

rm -rf "$PKG_WORK_DIR"
mkdir -p "$PKG_WORK_DIR/bin" "$PKG_WORK_DIR/vlc" "$PKG_WORK_DIR/libexec/vlc" "$PKG_WORK_DIR/share/vlc"

cat > "$PKG_WORK_DIR/appinfo.json" <<EOF
{
  "id": "$APP_ID",
  "version": "$APP_VERSION",
  "vendor": "VideoLAN",
  "type": "native",
  "main": "run.sh",
  "title": "VLC Media Player",
  "icon": "icon.png",
  "largeIcon": "largeIcon.png"
}
EOF

create_placeholder_png() {
    out="$1"
    cat <<'EOF' | base64 -d > "$out"
iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO7Z2ioAAAAASUVORK5CYII=
EOF
}

create_placeholder_png "$PKG_WORK_DIR/icon.png"
cp "$PKG_WORK_DIR/icon.png" "$PKG_WORK_DIR/largeIcon.png"

if [ -x "$VLC_ROOT/bin/vlc" ]; then
    cp "$VLC_ROOT/bin/vlc" "$PKG_WORK_DIR/bin/vlc"
else
    echo "Missing binary: $VLC_ROOT/bin/vlc"
    exit 1
fi

if [ -x "$VLC_ROOT/libexec/vlc/vlc-cache-gen" ]; then
    cp "$VLC_ROOT/libexec/vlc/vlc-cache-gen" "$PKG_WORK_DIR/libexec/vlc/vlc-cache-gen"
fi

if [ -x "$VLC_ROOT/libexec/vlc/vlc-preparser" ]; then
    cp "$VLC_ROOT/libexec/vlc/vlc-preparser" "$PKG_WORK_DIR/libexec/vlc/vlc-preparser"
fi

if [ -x "$VLC_ROOT/libexec/vlc/vlc-qt-check" ]; then
    cp "$VLC_ROOT/libexec/vlc/vlc-qt-check" "$PKG_WORK_DIR/libexec/vlc/vlc-qt-check"
fi

cp -a "$VLC_ROOT/lib" "$PKG_WORK_DIR/vlc/"
rm -rf "$PKG_WORK_DIR/vlc/lib/vlc/plugins"
cp -a "$VLC_PLUGIN_SRC" "$PKG_WORK_DIR/vlc/plugins"
find "$PKG_WORK_DIR/vlc/plugins" -name plugins.dat -delete

if [ -d "$VLC_ROOT/share/vlc" ]; then
    cp -a "$VLC_ROOT/share/vlc/." "$PKG_WORK_DIR/share/vlc/"
fi

if [ -d "$TOP_DIR/share/lua" ]; then
    rm -rf "$PKG_WORK_DIR/share/vlc/lua"
    cp -a "$TOP_DIR/share/lua" "$PKG_WORK_DIR/share/vlc/lua"
fi

if [ -n "$WEBOS_QT_RUNTIME_DIR" ]; then
    if [ ! -d "$WEBOS_QT_RUNTIME_DIR" ]; then
        echo "WEBOS_QT_RUNTIME_DIR does not exist: $WEBOS_QT_RUNTIME_DIR"
        exit 1
    fi

    mkdir -p "$PKG_WORK_DIR/qt6"
    [ -d "$WEBOS_QT_RUNTIME_DIR/plugins" ] && cp -a "$WEBOS_QT_RUNTIME_DIR/plugins" "$PKG_WORK_DIR/qt6/"
    [ -d "$WEBOS_QT_RUNTIME_DIR/qml" ]     && cp -a "$WEBOS_QT_RUNTIME_DIR/qml"     "$PKG_WORK_DIR/qt6/"

    if [ -d "$WEBOS_QT_RUNTIME_DIR/lib" ]; then
        find "$WEBOS_QT_RUNTIME_DIR/lib" -maxdepth 1 \( -type f -o -type l \) \
            \( -name 'libQt6*.so*' -o -name 'libicu*.so*' -o -name 'libxkbcommon*.so*' \
               -o -name 'libdouble-conversion*.so*' -o -name 'libpcre2-*.so*' -o -name 'libzstd*.so*' \) \
            -exec cp -a {} "$PKG_WORK_DIR/vlc/lib/" \;
    fi
fi

# ARM runtime closure: bundle toolchain libstdc++/libgcc and contrib libs.
#
# Root causes fixed:
#   GLIBCXX_3.4.32 not found  → buildroot-nc4 uses GCC 14; webOS TV has
#                               an older libstdc++.  Bundle the toolchain's.
#   libgmp.so.10 missing      → GnuTLS dep from contribs
#   libjpeg.so.8 missing      → JPEG dep from contribs
#   libpulse.so.0 missing     → PulseAudio dep from contribs

# 1. Toolchain libstdc++.so.6 and libgcc_s.so.1
if [ -n "${WEBOS_TOOLCHAIN:-}" ]; then
    _TC_ARM_LIB="$WEBOS_TOOLCHAIN/arm-webos-linux-gnueabi/lib"
    if [ -d "$_TC_ARM_LIB" ]; then
        for _lib in "$_TC_ARM_LIB"/libstdc++.so.6* "$_TC_ARM_LIB"/libgcc_s.so.1; do
            [ -f "$_lib" ] || continue
            _base="$(basename "$_lib")"
            [ -e "$PKG_WORK_DIR/vlc/lib/$_base" ] && continue
            cp "$_lib" "$PKG_WORK_DIR/vlc/lib/$_base"
        done
        if [ ! -e "$PKG_WORK_DIR/vlc/lib/libstdc++.so.6" ]; then
            _versioned="$(ls "$PKG_WORK_DIR/vlc/lib"/libstdc++.so.6.*.* 2>/dev/null | tail -1)"
            [ -n "$_versioned" ] && ln -s "$(basename "$_versioned")" "$PKG_WORK_DIR/vlc/lib/libstdc++.so.6"
        fi
        if [ -f "$PKG_WORK_DIR/vlc/lib/libgcc_s.so.1" ] && \
           [ ! -e "$PKG_WORK_DIR/vlc/lib/libgcc_s.so" ]; then
            ln -s libgcc_s.so.1 "$PKG_WORK_DIR/vlc/lib/libgcc_s.so"
        fi
    fi
fi

# 2. Contrib/deps shared libraries (libgmp, libjpeg, libgnutls, libpulse, …)
if [ -d "${WEBOS_DEPS_PREFIX:-}/lib" ]; then
    find "$WEBOS_DEPS_PREFIX/lib" -maxdepth 1 -type f -name '*.so.*' | \
    while IFS= read -r _dep; do
        _base="$(basename "$_dep")"
        case "$_base" in
            libvlc.so*|libvlccore.so*) continue ;;
            libc.so.*|libm.so.*|libdl.so.*|librt.so.*|\
            libpthread.so.*|libanl.so.*|libresolv.so.*|\
            libnsl.so.*|libutil.so.*) continue ;;
        esac
        [ -e "$PKG_WORK_DIR/vlc/lib/$_base" ] && continue
        cp "$_dep" "$PKG_WORK_DIR/vlc/lib/$_base"
    done
fi

if [ -d "$PKG_WORK_DIR/qt6/qml/QtTest" ]; then
    rm -rf "$PKG_WORK_DIR/qt6/qml/QtTest"
fi

cat > "$PKG_WORK_DIR/bin/qt.conf" <<'EOF'
[Paths]
Prefix=..
Plugins=qt6/plugins
QmlImports=qt6/qml
Libraries=vlc/lib
EOF

if [ -x "$PKG_WORK_DIR/libexec/vlc/vlc-cache-gen" ]; then
    "$PKG_WORK_DIR/libexec/vlc/vlc-cache-gen" "$PKG_WORK_DIR/vlc/plugins" >/dev/null 2>&1 || true
fi

cat > "$PKG_WORK_DIR/run.sh" <<EOF
#!/bin/sh
APP_DIR="/media/developer/apps/usr/palm/applications/__WEBOS_APP_ID__"
SYS_LIB_PATH="/usr/lib:/lib"
EOF

cat >> "$PKG_WORK_DIR/run.sh" <<'EOF'
export LD_LIBRARY_PATH="${APP_DIR}/vlc/lib:${SYS_LIB_PATH}:${LD_LIBRARY_PATH:-}"
export VLC_PLUGIN_PATH="${APP_DIR}/vlc/plugins"
export VLC_LIBEXEC_PATH="${APP_DIR}/libexec/vlc"
export VLC_DATA_PATH="${APP_DIR}/share/vlc"
VLC_INTF="${VLC_INTF:-qt}"
if [ -d "${APP_DIR}/qt6/plugins" ]; then
    export QT_PLUGIN_PATH="${APP_DIR}/qt6/plugins"
    export QT_QPA_PLATFORM_PLUGIN_PATH="${APP_DIR}/qt6/plugins/platforms"
    export QML2_IMPORT_PATH="${APP_DIR}/qt6/qml"
    export QT_QUICK_CONTROLS_STYLE=Basic
    export QT_NO_SETLOCALE=1
    export LANG=C.UTF-8
    export LC_ALL=C.UTF-8
    export QT_WAYLAND_SHELL_INTEGRATION=wl-shell
    export VLC_QT_SKIP_CHECK=1
    export QT_QPA_PLATFORM=wayland
    export VLC_VOUT="${VLC_VOUT:-wl_shm,wl_shell,xdg_shell,gles2,gl,any}"
fi

if [ -d /tmp/xdg ]; then
    export XDG_RUNTIME_DIR=/tmp/xdg
fi

if [ -S "${XDG_RUNTIME_DIR:-}/wayland-0" ]; then
    export WAYLAND_DISPLAY=wayland-0
fi

cd "$APP_DIR"

LOG_FILE="/tmp/vlc-sam-launch.log"
if [ "${1#\{}" != "$1" ]; then
    {
        echo "===== $(date '+%F %T') run.sh launch ====="
        echo "args: $*"
    } >> "$LOG_FILE"
    exec >> "$LOG_FILE" 2>&1
fi

EOF

cat >> "$PKG_WORK_DIR/run.sh" <<'EOF'
run_vlc() {
    if [ -x ./bin/vlc ]; then
        exec ./bin/vlc "$@"
    fi
    exit 127
}

EOF

cat >> "$PKG_WORK_DIR/run.sh" <<'EOF'
ARG1="${1:-}"
case "$ARG1" in
    ""|\{*\})
        run_vlc --intf="$VLC_INTF" --no-video-title-show --no-playlist-autostart ${VLC_VOUT:+--vout="$VLC_VOUT"}
        ;;
    *)
        run_vlc --intf="$VLC_INTF" --no-video-title-show --no-playlist-autostart ${VLC_VOUT:+--vout="$VLC_VOUT"} "$ARG1"
        ;;
esac
EOF

sed -i "s|__WEBOS_APP_ID__|$APP_ID|g" "$PKG_WORK_DIR/run.sh"

chmod +x "$PKG_WORK_DIR/run.sh"

if ! command -v ares-package >/dev/null 2>&1; then
    echo "ares-package not found. Install webOS CLI tools first."
    echo "Staged package directory: $PKG_WORK_DIR"
    exit 1
fi

mkdir -p "$OUT_DIR"
cd "$OUT_DIR"
rm -f "${APP_ID}_"*.ipk
ares-package "$PKG_WORK_DIR"

PKG_CREATED="$(ls -1t "${APP_ID}_"*.ipk 2>/dev/null | head -n 1)"
if [ -z "$PKG_CREATED" ]; then
    echo "IPK was not created."
    exit 1
fi

FINAL_PKG="${APP_ID}_${APP_VERSION}_${TARGET_ARCH}.ipk"
if [ "$PKG_CREATED" != "$FINAL_PKG" ]; then
    mv "$PKG_CREATED" "$FINAL_PKG"
fi

echo ""
echo "Created: $OUT_DIR/$FINAL_PKG"
ls -lh "$FINAL_PKG"
