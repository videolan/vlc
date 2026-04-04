#!/bin/sh
# This script is a compatibility shim.
# The canonical build system is build-webos.sh at the repository root.

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
TOP_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd -P)"

exec "$TOP_DIR/build-webos.sh" configure "$@"
