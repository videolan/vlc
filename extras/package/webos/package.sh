#!/bin/sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
. "$SCRIPT_DIR/env.build.sh"

APP_ID="${WEBOS_APP_ID:-org.videolan.vlc.webos}"
APP_VERSION="${WEBOS_APP_VERSION:-1.0.0}"
TARGET_ARCH="${WEBOS_TARGET_ARCH:-arm}"
DEPLOY_DIR="${WEBOS_DEPLOY_DIR:-$TOP_DIR/vlc-webos-deploy}"
OUT_DIR="${WEBOS_OUTPUT_DIR:-$TOP_DIR/webos-package}"
PKG_WORK_DIR="${WEBOS_PACKAGE_DIR:-$TOP_DIR/webos-package/stage}"

if [ ! -d "$DEPLOY_DIR" ]; then
    echo "Missing deploy directory: $DEPLOY_DIR"
    echo "Run: WEBOS_TOOLCHAIN=... PREFIX=/ DEPLOY_DIR=$DEPLOY_DIR ./build-webos.sh all"
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
mkdir -p "$PKG_WORK_DIR/bin" "$PKG_WORK_DIR/vlc" "$PKG_WORK_DIR/libexec/vlc"

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

cp -a "$VLC_ROOT/lib" "$PKG_WORK_DIR/vlc/"
rm -rf "$PKG_WORK_DIR/vlc/lib/vlc/plugins"
cp -a "$VLC_PLUGIN_SRC" "$PKG_WORK_DIR/vlc/plugins"
find "$PKG_WORK_DIR/vlc/plugins" -name plugins.dat -delete

if [ -x "$PKG_WORK_DIR/libexec/vlc/vlc-cache-gen" ]; then
    "$PKG_WORK_DIR/libexec/vlc/vlc-cache-gen" "$PKG_WORK_DIR/vlc/plugins" >/dev/null 2>&1 || true
fi

cat > "$PKG_WORK_DIR/run.sh" <<'EOF'
#!/bin/sh
APP_DIR="/media/developer/apps/usr/palm/applications/__WEBOS_APP_ID__"
export LD_LIBRARY_PATH="${APP_DIR}/vlc/lib:${LD_LIBRARY_PATH}"
export VLC_PLUGIN_PATH="${APP_DIR}/vlc/plugins"
export VLC_LIBEXEC_PATH="${APP_DIR}/libexec/vlc"

if [ -z "${XDG_RUNTIME_DIR}" ] && [ -S /tmp/xdg/wayland-0 ]; then
    export XDG_RUNTIME_DIR=/tmp/xdg
fi

if [ -z "${WAYLAND_DISPLAY}" ] && [ -S "${XDG_RUNTIME_DIR}/wayland-0" ]; then
    export WAYLAND_DISPLAY=wayland-0
fi

cd "$APP_DIR"

ARG1="${1:-}"
case "$ARG1" in
    ""|\{*\})
        exec ./bin/vlc --intf=dummy --no-video-title-show --no-playlist-autostart
        ;;
    *)
        exec ./bin/vlc --intf=dummy --no-video-title-show --no-playlist-autostart "$ARG1"
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
