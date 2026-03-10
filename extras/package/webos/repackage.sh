#!/bin/bash
set -e

export WEBOS_QT_RUNTIME_DIR="$HOME/qt6-webos/target/usr"
export WEBOS_DEPLOY_DIR="$HOME/vlc-webos-deploy"
# Use Linux filesystem paths so ares-package can work with them
export WEBOS_OUTPUT_DIR="$HOME/webos-package"
export WEBOS_PACKAGE_DIR="$HOME/webos-package/stage"

cd /mnt/c/Repos/vlc
echo "=== Running package.sh ==="
echo "WEBOS_OUTPUT_DIR: $WEBOS_OUTPUT_DIR"
echo "WEBOS_PACKAGE_DIR: $WEBOS_PACKAGE_DIR"
extras/package/webos/package.sh 2>&1
