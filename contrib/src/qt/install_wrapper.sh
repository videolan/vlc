#!/usr/bin/env sh
# Copyright (C) 2022 Videolabs
# This file is distributed under the same license as the vlc package.
set -e

SCRIPT_DIR="$(cd "$(dirname  "$0" )" && pwd -P)"
SOURCE="$1"
DEST="$2"

install -m 644 -p $SOURCE $DEST

# Filter pkg-config files only
if [ "${SOURCE##*.}" != 'pc' ]; then
    exit 0
fi

"${SCRIPT_DIR}/../pkg-static.sh" "${DEST}"

# Filter pkg-config files that are not installed in the main pkg-config folder
if [ "$(dirname $2)" -ef "${VLC_PREFIX}/lib/pkgconfig" ]; then
    exit 0
fi

pkgconfigdir="$(cd "$(dirname "${DEST}")" && pwd -P)"

# Filter packages installed in a pkgconfig/ folder
if [ "$(basename "${pkgconfigdir}")" = "pkgconfig" ]; then
    exit 0
fi

sed -i.orig "s,libdir=.*,libdir=${pkgconfigdir}," "${DEST}"
mkdir -p "${VLC_PREFIX}/lib/pkgconfig"
cp "${DEST}" "${VLC_PREFIX}/lib/pkgconfig/"
