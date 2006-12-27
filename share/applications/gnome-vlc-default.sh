#!/bin/sh
# share/applications/gnome-vlc-default.sh
# $Id$
#
# Copyright (C) 2006, VideoLAN team
# Authors: Torsten Spindler
#          Jean-Paul Saman <jpsaman _at_ videolan dot org>
#
# Set vlc as default player on the gnome desktop
# Tested on Ubuntu 6.10 Edgy
#
GCONFTOOL=gconftool
if test -x /usr/bin/gconftool; then
	GCONFTOOL=/usr/bin/gconftool
	OPTS="-t string -s"
fi
if test -x /usr/bin/gconftool-2; then
	GCONFTOOL=/usr/bin/gconftool-2
	OPTS="--type string --set"
fi
$GCONFTOOL $OPTS /desktop/gnome/volume_manager/autoplay_vcd_command "vlc %m"
$GCONFTOOL $OPTS /desktop/gnome/volume_manager/autoplay_dvd_command "vlc %m"
$GCONFTOOL $OPTS /desktop/gnome/url-handlers/mms "vlc %s"
$GCONFTOOL $OPTS /desktop/gnome/url-handlers/mmsh "vlc %s"
$GCONFTOOL $OPTS /desktop/gnome/url-handlers/rtsp "vlc %s"
$GCONFTOOL $OPTS /desktop/gnome/url-handlers/rtp "vlc %s"

exit 0
