#!/bin/sh
APP_DIR="/media/developer/apps/usr/palm/applications/org.videolan.vlc.webos"
SYS_LIB_PATH="/usr/lib:/lib"
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
    export QT_WAYLAND_SHELL_INTEGRATION=wlshell
    export VLC_QT_SKIP_CHECK=1
    export QT_QPA_PLATFORM=wayland
    export VLC_VOUT="${VLC_VOUT:-wl_shm,wl_shell,xdg_shell,gles2,gl,any}"
fi

# Always set these unconditionally (full path avoids XDG_RUNTIME_DIR dependency)
export XDG_RUNTIME_DIR=/tmp/xdg
export WAYLAND_DISPLAY=wayland-0

# Wayland/Qt debug
export WAYLAND_DEBUG=1
export WAYLAND_DEBUG_PATH=/tmp/wayland-debug.log
export QT_LOGGING_RULES='qt.qpa.wayland*=true'
export QT_DEBUG_PLUGINS=1
# Try software hardware integration to avoid EGL failure
export QT_WAYLAND_HARDWARE_INTEGRATION=wayland-shm

cd "$APP_DIR"

LOG_FILE="/tmp/vlc-sam-launch.log"
if [ "${1#\{}" != "$1" ]; then
    {
        echo "===== $(date '+%F %T') run.sh launch ====="
        echo "args: $*"
        echo "=== id/groups ==="
        id
        echo "=== SMACK label of this process ==="
        cat /proc/self/attr/current 2>/dev/null; echo "(end)"
        echo "=== SMACK label of wayland socket ==="
        cat /proc/self/attr/sockcreate 2>/dev/null; echo "(end)"
        echo "=== initial env (WAYLAND/XDG/DISPLAY before override) ==="
        echo "ORIG_WAYLAND_DISPLAY=${WAYLAND_DISPLAY:-<unset>}"
        echo "ORIG_XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-<unset>}"
        echo "=== /tmp/xdg contents ==="
        ls -la /tmp/xdg/ 2>&1
        echo "=== socket check ==="
        test -S /tmp/xdg/wayland-0 && echo "socket exists" || echo "socket MISSING"
        test -r /tmp/xdg/wayland-0 && echo "socket readable" || echo "socket NOT READABLE"
        test -w /tmp/xdg/wayland-0 && echo "socket writable" || echo "socket NOT WRITABLE"
        echo "=== XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR WAYLAND_DISPLAY=$WAYLAND_DISPLAY (after override) ==="
        echo "=== python socket connect test ==="
        python3 /tmp/test-wayland-connect.py 2>&1 || echo "python3 test FAILED/unavailable"
        echo "=== all env vars ==="
        env | sort
    } >> "$LOG_FILE"
    exec >> "$LOG_FILE" 2>&1
fi

run_vlc() {
    if [ -x ./bin/vlc ]; then
        exec ./bin/vlc "$@"
    fi
    exit 127
}

ARG1="${1:-}"
case "$ARG1" in
    ""|\{*\})
        run_vlc --intf="${VLC_INTF}" --no-video-title-show --no-playlist-autostart ${VLC_VOUT:+--vout="${VLC_VOUT}"}
        ;;
    *)
        run_vlc --intf="${VLC_INTF}" --no-video-title-show --no-playlist-autostart ${VLC_VOUT:+--vout="${VLC_VOUT}"} "$ARG1"
        ;;
esac
