#!/bin/sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
. "$SCRIPT_DIR/env.build.sh"

BUILD_DIR="${BUILD_DIR:-$TOP_DIR/build-webos}"

cd "$TOP_DIR"

if [ ! -x ./configure ]; then
    ./bootstrap
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

"$SCRIPT_DIR/configure.sh" "$@"
make -j"$JOBS"
