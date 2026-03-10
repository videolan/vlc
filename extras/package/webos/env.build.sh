#!/bin/sh
# Minimal environment shared by packaging utilities (package.sh, repackage.sh).
# Build/configure has moved to build-webos.sh at the repository root.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd -P)"
TOP_DIR="$(cd "$SCRIPT_DIR/../../.." && pwd -P)"
: "${JOBS:=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"

export TOP_DIR JOBS
