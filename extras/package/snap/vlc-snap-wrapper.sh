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

export LD_LIBRARY_PATH="$SNAP/usr/lib:$SNAP/usr/lib/vlc:$LD_LIBRARY_PATH"
export VLC_PLUGIN_PATH="$SNAP/usr/lib/vlc/plugins"

# KDE specific
## Do not start slaves through klauncher but fork them directly.
export KDE_FORK_SLAVES=1
## Neon PATCH! make KIO look for slaves in a dynamic location depending on $SNAP
#export KF5_LIBEXEC_DIR=$SNAP/usr/lib/$ARCH/libexec/kf5

# Link the aacs directory from $HOME #28017
if [ ! -L "$HOME/.config/aacs" ]; then
    ln -s $SNAP_REAL_HOME/.config/aacs $HOME/.config/aacs
fi

exec $SNAP/usr/bin/vlc --config=$SNAP_USER_COMMON/vlcrc "$@"
