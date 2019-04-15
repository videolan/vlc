#!/bin/bash
case "$SNAP_ARCH" in
	"amd64") ARCH='x86_64-linux-gnu'
	;;
	"i386") ARCH='i386-linux-gnu'
	;;
	*)
		echo "Unsupported architecture for this app build"
		exit 1
	;;
esac

VENDOR=$(glxinfo | grep "OpenGL vendor")

if [[ $VENDOR == *"Intel"* ]]; then
  export VDPAU_DRIVER_PATH="$SNAP/usr/lib/$ARCH/dri"
  export LIBVA_DRIVERS_PATH="$SNAP/usr/lib/$ARCH/dri"
fi

if [[ $VENDOR == *"NVIDIA"* ]]; then
  export VDPAU_DRIVER_PATH="/var/lib/snapd/lib/gl/vdpau"
elif [[ $VENDOR == *"X.Org"* ]]; then
  export VDPAU_DRIVER_PATH="/usr/lib/$ARCH/vdpau/"
fi

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$SNAP/usr/lib/vlc"

# KDE specific
## Do not start slaves through klauncher but fork them directly.
export KDE_FORK_SLAVES=1
## Neon PATCH! make KIO look for slaves in a dynamic location depending on $SNAP
export KF5_LIBEXEC_DIR=$SNAP/usr/lib/$ARCH/libexec/kf5

# set QML2 import path for Qt UI
export QML2_IMPORT_PATH="$QML2_IMPORT_PATH:$SNAP/usr/lib/x86_64-linux-gnu/qt5/qml/"

exec $SNAP/usr/bin/vlc --config=$SNAP_USER_COMMON/vlcrc "$@"
