#!/bin/sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
. "$SCRIPT_DIR/env.build.sh"

OPTIONS="
    --host=$WEBOS_HOST
    --prefix=$WEBOS_PREFIX
    --disable-debug
    --disable-dbus
    --disable-xcb
    --disable-wayland
    --disable-libdrm
    --without-x
"

if [ -d "$WEBOS_CONTRIB_DIR" ]; then
    OPTIONS="$OPTIONS --with-contrib=$WEBOS_CONTRIB_DIR"
fi

exec sh "$SCRIPT_DIR/../../../configure" $OPTIONS "$@"
