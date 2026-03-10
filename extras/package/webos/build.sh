#!/bin/sh
# Thin wrapper — delegates to build-webos.sh in the same directory.
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
exec "$SCRIPT_DIR/build-webos.sh" "${1:-all}" "$@"
