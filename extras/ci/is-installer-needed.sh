#!/usr/bin/env bash
# Copyright (C) Marvin Scholz / Steve Lhomme
#
# License: see COPYING
#
# Check whether some installer files changed of a nightly is built (needed for CI)
# Return "yes" if the installer should be (re)built and "" otherwise
set -e

# Array of paths that should trigger contrib rebuilds
VLC_CONTRIB_REBUILD_PATHS=()
# Revision from which to start look for changes (backwards in time)
VLC_START_REVISION="HEAD"
# Upstream branch to compare changes with
VLC_UPSTREAM_BRANCH="origin/master"

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

case $1 in
    nightly-*)
        echo "yes"
        exit 0
        ;;
    win32*|win64*|uwp*)
        # check if the NSIX/WIX folders changed
        # check if the wix contribs changed
        # check if gitlab-ci.yml changed as it may influence the nightly build
        VLC_CONTRIB_REBUILD_PATHS+=( "extras/ci/gitlab-ci.yml" "extras/package/win32/NSIS" "extras/package/win32/msi" "contrib/src/wix" "contrib/src/wixlzx" )
        ;;
    *)
        echo ""
        exit 0
esac

for path in "${VLC_CONTRIB_REBUILD_PATHS[@]}"; do
    CHANGED_FILES_IN_PATH=$(git diff --name-only ${VLC_UPSTREAM_BRANCH}..${VLC_START_REVISION} "${path}")
    if [ -n "${CHANGED_FILES_IN_PATH}" ]; then
        echo "yes"
        break
    fi
done

exit 0
