#!/bin/bash
# pkg-config wrapper for ARM webOS cross-compilation
# Sets PKG_CONFIG_LIBDIR and PKG_CONFIG_SYSROOT_DIR to the ARM sysroot

SYSROOT=/home/gabor/vlc-webos-build/sdk/arm-webos-linux-gnueabi_sdk-buildroot/arm-webos-linux-gnueabi/sysroot

export PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
export PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig"

exec /usr/bin/pkg-config "$@"
