#!/bin/sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
TOP_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd -P)"

: "${WEBOS_HOST:=arm-webos-linux-gnueabi}"
: "${WEBOS_PREFIX:=/media/developer/apps/usr/palm/applications/org.videolan.vlc}"
: "${WEBOS_TOOLCHAIN:=}"
: "${WEBOS_PKG_CONFIG_PATH:=}"
: "${WEBOS_CONTRIB_DIR:=$TOP_DIR/contrib/$WEBOS_HOST}"
: "${JOBS:=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"

if [ -n "$WEBOS_TOOLCHAIN" ] && [ -d "$WEBOS_TOOLCHAIN/bin" ]; then
    PATH="$WEBOS_TOOLCHAIN/bin:$PATH"
fi

if [ -n "$WEBOS_PKG_CONFIG_PATH" ]; then
    PKG_CONFIG_PATH="$WEBOS_PKG_CONFIG_PATH"
    export PKG_CONFIG_PATH
fi

export WEBOS_HOST WEBOS_PREFIX WEBOS_TOOLCHAIN WEBOS_CONTRIB_DIR JOBS PATH TOP_DIR
