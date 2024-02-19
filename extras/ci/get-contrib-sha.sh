#!/usr/bin/env bash
# Copyright (C) Marvin Scholz
#
# License: see COPYING
#
# Get latest SHA that changed contribs (needed for CI)
set -e

# Array of paths that should trigger contrib rebuilds
VLC_CONTRIB_REBUILD_PATHS=("extras/tools" "contrib" "extras/ci")
# Revision from which to start look for changes (backwards in time)
VLC_START_REVISION="HEAD"

# Print error message and terminate script with status 1
# Arguments:
#   Message to print
abort_err()
{
    echo "ERROR: $1" >&2
    exit 1
}

command -v "git" >/dev/null 2>&1 || abort_err "Git was not found!"

# VLC source root directory
VLC_SRC_ROOT_DIR=$(git rev-parse --show-toplevel)

[ -n "${VLC_SRC_ROOT_DIR}" ] || abort_err "This script must be run in the VLC Git repo and git must be available"
[ -f "${VLC_SRC_ROOT_DIR}/src/libvlc.h" ] || abort_err "This script must be run in the VLC Git repository"

VLC_BUILD_TARGET="extras/package"
case $1 in
    win32*|win64*|uwp*)
        VLC_BUILD_TARGET="extras/package/win32"
        ;;
    debian*)
        VLC_BUILD_TARGET=""
        ;;
    ios*|tvos*)
        VLC_BUILD_TARGET="extras/package/apple"
        ;;
    macos*)
        VLC_BUILD_TARGET="extras/package/macosx"
        ;;
    raspbian*)
        VLC_BUILD_TARGET="extras/package/raspberry"
        ;;
    snap*)
        VLC_BUILD_TARGET="extras/package/snap"
        ;;
    wasm*)
        VLC_BUILD_TARGET="extras/package/wasm-emscripten"
        ;;
    android*)
        VLC_BUILD_TARGET=""
        ;;
esac

VLC_LAST_CONTRIB_SHA=$(
    cd "$VLC_SRC_ROOT_DIR" &&
    git rev-list -1 "${VLC_START_REVISION}" -- "${VLC_CONTRIB_REBUILD_PATHS[@]}" "${VLC_BUILD_TARGET}"
)

[ -n "${VLC_LAST_CONTRIB_SHA}" ] || abort_err "Failed to determine last contrib SHA using Git!"

echo "${VLC_LAST_CONTRIB_SHA}"
