#!/bin/sh

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$SNAP/lib/vlc"

exec desktop-launch vlc "$@"
