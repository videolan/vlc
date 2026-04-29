#!/bin/bash
#KF6 already ships VLC and set the pluging path
#ensure we don't use their libraries
export LD_LIBRARY_PATH="$SNAP/usr/lib:$SNAP/usr/lib/vlc:$LD_LIBRARY_PATH"
unset VLC_PLUGIN_PATH

# KDE specific
## Do not start slaves through klauncher but fork them directly.
export KDE_FORK_SLAVES=1
## Neon PATCH! make KIO look for slaves in a dynamic location depending on $SNAP
#export KF5_LIBEXEC_DIR=$SNAP/usr/lib/$ARCH/libexec/kf5

#libva+wayland is broken with gpu2404 base snap, force X11
export QT_QPA_PLATFORM=xcb

# Link the aacs directory from $HOME #28017
if [ ! -L "$HOME/.config/aacs" ]; then
    ln -s $SNAP_REAL_HOME/.config/aacs $HOME/.config/aacs
fi

exec $SNAP/usr/bin/vlc --config=$SNAP_USER_COMMON/vlcrc "$@"
