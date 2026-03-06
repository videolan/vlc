#!/bin/sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
. "$SCRIPT_DIR/env.build.sh"

APP_ID="${WEBOS_APP_ID:-org.videolan.vlc.webos}"
APP_VERSION="${WEBOS_APP_VERSION:-1.0.0}"
TARGET_ARCH="${WEBOS_TARGET_ARCH:-arm}"
if [ -n "${WEBOS_DEPLOY_DIR:-}" ]; then
    DEPLOY_DIR="$WEBOS_DEPLOY_DIR"
elif [ "$TARGET_ARCH" = "x86_64" ]; then
    DEPLOY_DIR="$TOP_DIR/vlc-webos-deploy-x86_64"
else
    DEPLOY_DIR="$TOP_DIR/vlc-webos-deploy"
fi
OUT_DIR="${WEBOS_OUTPUT_DIR:-$TOP_DIR/webos-package}"
PKG_WORK_DIR="${WEBOS_PACKAGE_DIR:-$TOP_DIR/webos-package/stage}"
WEBOS_QT_RUNTIME_DIR="${WEBOS_QT_RUNTIME_DIR:-}"
WEBOS_PACKAGE_PROFILE="${WEBOS_PACKAGE_PROFILE:-full}"

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

if [ "$TARGET_ARCH" = "x86_64" ] && [ "$WEBOS_PACKAGE_PROFILE" = "minimal" ]; then
    rm -rf \
        "$PKG_WORK_DIR/vlc/plugins/access_output" \
        "$PKG_WORK_DIR/vlc/plugins/audio_filter" \
        "$PKG_WORK_DIR/vlc/plugins/audio_mixer" \
        "$PKG_WORK_DIR/vlc/plugins/lua" \
        "$PKG_WORK_DIR/vlc/plugins/meta_engine" \
        "$PKG_WORK_DIR/vlc/plugins/mux" \
        "$PKG_WORK_DIR/vlc/plugins/nvdec" \
        "$PKG_WORK_DIR/vlc/plugins/services_discovery" \
        "$PKG_WORK_DIR/vlc/plugins/spu" \
        "$PKG_WORK_DIR/vlc/plugins/stream_extractor" \
        "$PKG_WORK_DIR/vlc/plugins/stream_filter" \
        "$PKG_WORK_DIR/vlc/plugins/stream_out" \
        "$PKG_WORK_DIR/vlc/plugins/text_renderer" \
        "$PKG_WORK_DIR/vlc/plugins/vaapi" \
        "$PKG_WORK_DIR/vlc/plugins/video_chroma" \
        "$PKG_WORK_DIR/vlc/plugins/video_filter" \
        "$PKG_WORK_DIR/vlc/plugins/video_splitter" \
        "$PKG_WORK_DIR/vlc/plugins/visualization"

    rm -f \
        "$PKG_WORK_DIR/vlc/plugins/codec/libvaapi_plugin.so" \
        "$PKG_WORK_DIR/vlc/plugins/codec/libvaapi_plugin.la" \
        "$PKG_WORK_DIR/vlc/plugins/demux/libavformat_plugin.so" \
        "$PKG_WORK_DIR/vlc/plugins/demux/libavformat_plugin.la" \
        "$PKG_WORK_DIR/vlc/plugins/demux/libadaptive_plugin.so" \
        "$PKG_WORK_DIR/vlc/plugins/demux/libadaptive_plugin.la" \
        "$PKG_WORK_DIR/vlc/plugins/packetizer/libpacketizer_avparser_plugin.so" \
        "$PKG_WORK_DIR/vlc/plugins/packetizer/libpacketizer_avparser_plugin.la" \
        "$PKG_WORK_DIR/vlc/plugins/access/libdcp_plugin.so" \
        "$PKG_WORK_DIR/vlc/plugins/access/libdcp_plugin.la" \
        "$PKG_WORK_DIR/vlc/plugins/access/libnfs_plugin.so" \
        "$PKG_WORK_DIR/vlc/plugins/access/libnfs_plugin.la" \
        "$PKG_WORK_DIR/vlc/plugins/access/libaccess_srt_plugin.so" \
        "$PKG_WORK_DIR/vlc/plugins/access/libaccess_srt_plugin.la" \
        "$PKG_WORK_DIR/vlc/plugins/access/libcdda_plugin.so" \
        "$PKG_WORK_DIR/vlc/plugins/access/libcdda_plugin.la" \
        "$PKG_WORK_DIR/vlc/plugins/access/libavio_plugin.so" \
        "$PKG_WORK_DIR/vlc/plugins/access/libavio_plugin.la" \
        "$PKG_WORK_DIR/vlc/plugins/access/libsmb2_plugin.so" \
        "$PKG_WORK_DIR/vlc/plugins/access/libsmb2_plugin.la" \
        "$PKG_WORK_DIR/vlc/plugins/access/librist_plugin.so" \
        "$PKG_WORK_DIR/vlc/plugins/access/librist_plugin.la" \
        "$PKG_WORK_DIR/vlc/plugins/misc/libmedialibrary_plugin.so" \
        "$PKG_WORK_DIR/vlc/plugins/misc/libmedialibrary_plugin.la" \
        "$PKG_WORK_DIR/vlc/plugins/misc/libgnutls_plugin.so" \
        "$PKG_WORK_DIR/vlc/plugins/misc/libgnutls_plugin.la" \
        "$PKG_WORK_DIR/vlc/plugins/video_output/libplacebo_vk_plugin.so" \
        "$PKG_WORK_DIR/vlc/plugins/video_output/libplacebo_vk_plugin.la"
fi

copy_host_lib() {
    local src="$1"
    local base real_src real_base soname

    [ -f "$src" ] || return 0
    base="$(basename "$src")"
    real_src="$(readlink -f "$src")"
    real_base="$(basename "$real_src")"

    if [ ! -e "$PKG_WORK_DIR/vlc/lib/$real_base" ]; then
        cp -a "$real_src" "$PKG_WORK_DIR/vlc/lib/$real_base"
    fi

    if [ "$base" != "$real_base" ] && [ ! -e "$PKG_WORK_DIR/vlc/lib/$base" ]; then
        ln -s "$real_base" "$PKG_WORK_DIR/vlc/lib/$base"
    fi

    soname="$(objdump -p "$real_src" 2>/dev/null | awk '/SONAME/ { print $2; exit }')"
    if [ -n "$soname" ] && [ "$soname" != "$real_base" ] && [ ! -e "$PKG_WORK_DIR/vlc/lib/$soname" ]; then
        ln -s "$real_base" "$PKG_WORK_DIR/vlc/lib/$soname"
    fi
}

bundle_runtime_closure() {
    local pending done current dep base

    pending="$(mktemp)"
    done="$(mktemp)"
    printf '%s\n' "$@" > "$pending"

    while [ -s "$pending" ]; do
        current="$(head -n 1 "$pending")"
        sed -i '1d' "$pending"

        [ -f "$current" ] || continue
        grep -Fxq "$current" "$done" && continue
        echo "$current" >> "$done"

        ldd "$current" 2>/dev/null | awk '/=> \/.*/ { print $3 }' | while IFS= read -r dep; do
            [ -f "$dep" ] || continue
            base="$(basename "$dep")"
            case "$base" in
                ld-linux-*.so*|libc.so.*|libm.so.*|libdl.so.*|librt.so.*|libpthread.so.*|libanl.so.*)
                    continue
                    ;;
            esac

            copy_host_lib "$dep"
            if ! grep -Fxq "$dep" "$done"; then
                echo "$dep" >> "$pending"
            fi
        done
    done

    rm -f "$pending" "$done"
}

if [ "$TARGET_ARCH" = "x86_64" ]; then
    HOST_MULTIARCH="$(gcc -print-multiarch 2>/dev/null || true)"
    [ -n "$HOST_MULTIARCH" ] || HOST_MULTIARCH="x86_64-linux-gnu"

    mkdir -p "$PKG_WORK_DIR/qt6"
    if [ -d "/usr/lib/${HOST_MULTIARCH}/qt6/plugins" ]; then
        cp -a "/usr/lib/${HOST_MULTIARCH}/qt6/plugins" "$PKG_WORK_DIR/qt6/"
    fi
    if [ -d "/usr/lib/${HOST_MULTIARCH}/qt6/qml" ]; then
        cp -a "/usr/lib/${HOST_MULTIARCH}/qt6/qml" "$PKG_WORK_DIR/qt6/"
    fi

    QT_INPUTS=""
    for candidate in \
        "$VLC_ROOT/lib/vlc/plugins/gui/libqt_plugin.so" \
        "$VLC_ROOT/lib/vlc/plugins/gui/libqt_wayland_plugin.so" \
        "$PKG_WORK_DIR/qt6/qml/QtQuick/Templates/libqtquicktemplates2plugin.so" \
        "$PKG_WORK_DIR/qt6/qml/QtQuick/Controls/libqtquickcontrols2plugin.so" \
        "$PKG_WORK_DIR/qt6/qml/QtQuick/Controls/Basic/libqtquickcontrols2basicstyleplugin.so" \
        "$PKG_WORK_DIR/qt6/qml/QtQuick/Controls/Fusion/libqtquickcontrols2fusionstyleplugin.so" \
        "$PKG_WORK_DIR/qt6/qml/QtQuick/Window/libquickwindowplugin.so" \
        "$PKG_WORK_DIR/qt6/plugins/platforms/libqwayland-egl.so" \
        "$PKG_WORK_DIR/qt6/plugins/platforms/libqwayland-generic.so" \
        "$PKG_WORK_DIR/qt6/plugins/platforms/libqxcb.so"; do
        if [ -f "$candidate" ]; then
            QT_INPUTS="$QT_INPUTS $candidate"
        fi
    done

    if [ -f "/usr/lib/${HOST_MULTIARCH}/libidn.so.12" ]; then
        copy_host_lib "/usr/lib/${HOST_MULTIARCH}/libidn.so.12"
    fi

    for xlib in \
        "/usr/lib/${HOST_MULTIARCH}/libX11.so.6" \
        "/usr/lib/${HOST_MULTIARCH}/libXext.so.6"; do
        if [ -f "$xlib" ]; then
            copy_host_lib "$xlib"
        fi
    done

    if [ -f "/usr/lib/${HOST_MULTIARCH}/libvdpau.so.1" ]; then
        copy_host_lib "/usr/lib/${HOST_MULTIARCH}/libvdpau.so.1"
    fi

    for qtlib in \
        "/usr/lib/${HOST_MULTIARCH}/libQt6QuickLayouts.so.6" \
        "/usr/lib/${HOST_MULTIARCH}/libQt6QuickTemplates2.so.6" \
        "/usr/lib/${HOST_MULTIARCH}/libQt6QuickControls2.so.6" \
        "/usr/lib/${HOST_MULTIARCH}/libQt6QuickControls2Impl.so.6" \
        "/usr/lib/${HOST_MULTIARCH}/libQt6QuickControls2BasicStyleImpl.so.6" \
        "/usr/lib/${HOST_MULTIARCH}/libQt6Svg.so.6"; do
        if [ -f "$qtlib" ]; then
            copy_host_lib "$qtlib"
        fi
    done

    if [ -n "$QT_INPUTS" ]; then
        # shellcheck disable=SC2086
        bundle_runtime_closure $QT_INPUTS
    fi

elif [ "$TARGET_ARCH" = "arm" ] && [ -n "$WEBOS_QT_RUNTIME_DIR" ]; then
    if [ ! -d "$WEBOS_QT_RUNTIME_DIR" ]; then
        echo "WEBOS_QT_RUNTIME_DIR does not exist: $WEBOS_QT_RUNTIME_DIR"
        exit 1
    fi

    mkdir -p "$PKG_WORK_DIR/qt6"
    if [ -d "$WEBOS_QT_RUNTIME_DIR/plugins" ]; then
        cp -a "$WEBOS_QT_RUNTIME_DIR/plugins" "$PKG_WORK_DIR/qt6/"
    fi
    if [ -d "$WEBOS_QT_RUNTIME_DIR/qml" ]; then
        cp -a "$WEBOS_QT_RUNTIME_DIR/qml" "$PKG_WORK_DIR/qt6/"
    fi

    if [ -d "$WEBOS_QT_RUNTIME_DIR/lib" ]; then
        find "$WEBOS_QT_RUNTIME_DIR/lib" -maxdepth 1 -type f \( -name 'libQt6*.so*' -o -name 'libicu*.so*' -o -name 'libxkbcommon*.so*' -o -name 'libdouble-conversion*.so*' -o -name 'libpcre2-*.so*' -o -name 'libzstd*.so*' \) -exec cp -a {} "$PKG_WORK_DIR/vlc/lib/" \;
    fi
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

cat > "$PKG_WORK_DIR/run.sh" <<'EOF'
#!/bin/sh
APP_DIR="/media/developer/apps/usr/palm/applications/__WEBOS_APP_ID__"
SYS_LIB_PATH="/usr/lib/x86_64-linux-gnu"
export LD_LIBRARY_PATH="${APP_DIR}/vlc/lib:${SYS_LIB_PATH}:${LD_LIBRARY_PATH}"
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
    export QT_WAYLAND_SHELL_INTEGRATION=xdg-shell
    export VLC_QT_SKIP_CHECK=1
    export QT_QPA_PLATFORM=wayland
    export VLC_VOUT="${VLC_VOUT:-wl_shm,wl_shell,xdg_shell,gles2,gl,any}"
fi

if [ -d /tmp/xdg ]; then
    export XDG_RUNTIME_DIR=/tmp/xdg
fi

if [ -S "${XDG_RUNTIME_DIR}/wayland-0" ]; then
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

run_vlc() {
    if [ -x ./bin/vlc ]; then
        if [ ! -e /lib64/ld-linux-x86-64.so.2 ] && [ -x /lib/ld-linux-x86-64.so.2 ]; then
            for helper in vlc-preparser vlc-cache-gen vlc-qt-check; do
                if [ -x "${APP_DIR}/libexec/vlc/${helper}" ]; then
                    if [ "$(head -n 1 "${APP_DIR}/libexec/vlc/${helper}" 2>/dev/null || true)" != "#!/bin/sh" ]; then
                        mv -f "${APP_DIR}/libexec/vlc/${helper}" "${APP_DIR}/libexec/vlc/${helper}.real"
                    fi

                    if [ ! -x "${APP_DIR}/libexec/vlc/${helper}.real" ]; then
                        continue
                    fi

                    cat > "${APP_DIR}/libexec/vlc/${helper}" <<HELPER_EOF
#!/bin/sh
exec /lib/ld-linux-x86-64.so.2 --library-path "${APP_DIR}/vlc/lib:${SYS_LIB_PATH}:/lib:/usr/lib" "${APP_DIR}/libexec/vlc/${helper}.real" "\$@"
HELPER_EOF
                    chmod +x "${APP_DIR}/libexec/vlc/${helper}"
                fi
            done
            exec /lib/ld-linux-x86-64.so.2 --library-path "${APP_DIR}/vlc/lib:${SYS_LIB_PATH}:/lib:/usr/lib" ./bin/vlc "$@"
        fi
        exec ./bin/vlc "$@"
    fi
    exit 127
}

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
