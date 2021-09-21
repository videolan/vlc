#!/usr/bin/env sh

SCRIPTDIR="$(cd "$(dirname "$0")" || exit 1; pwd)"
VLC_SRC_DIR="$(cd "${SCRIPTDIR}/../../../" || exit 1; pwd)"
PROJECTDIR="$(pwd)/xcodeproj/"

export VLC_SRC_DIR
mkdir -p "${PROJECTDIR}" && cd "${PROJECTDIR}" || exit 1

xcodegen generate --project . --spec "${VLC_SRC_DIR}/extras/package/apple/xcodegen.yml" --project-root .
