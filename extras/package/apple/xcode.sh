#!/usr/bin/env bash

SCRIPTDIR="$(cd $(dirname "$0"); pwd)"
SRCROOT="${SCRIPTDIR}/../../../"

# https://developer.apple.com/documentation/xcode-release-notes/build-system-release-notes-for-xcode-10
# > The new build system passes undefined_arch as the value for the ARCH
# > environment variable when running shell script build phases. The value was
# > previously not well defined. Any shell scripts depending on this value must
# > behave correctly for all defined architectures being built, available via
# > the ARCHS environment variable.

MAKEFLAGS="-j6"

for arch in $ARCHS; do
    echo "\n==========\n"
    echo "Building platform ${PLATFORM_NAME} for arch ${arch}"
    BUILD_DIR="${BUILT_PRODUCTS_DIR}/build-${PLATFORM_NAME}-${arch}/"
    if [ ! -f "${BUILD_DIR}/build/config.h" ]; then
        mkdir -p "${BUILD_DIR}"
        (cd "${BUILD_DIR}" && env -i V=1 VERBOSE=1 "${SCRIPTDIR}/build.sh" \
            --enable-shared --enable-merge-plugins --enable-bitcode \
            --arch="${arch}" --sdk="${PLATFORM_NAME}")
    else
        (cd "${BUILD_DIR}/build/" && env -i ./compile V=1 VERBOSE=1)
    fi
done
