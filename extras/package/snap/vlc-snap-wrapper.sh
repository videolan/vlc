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

exec $SNAP/usr/bin/vlc "$@"
