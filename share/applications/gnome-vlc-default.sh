#!/bin/sh
# Set vlc as default player on the gnome desktop
# Tested on Ubuntu 6.10 Edgy
gconftool -t string -s /desktop/gnome/volume_manager/autoplay_vcd_command "vlc %m"
gconftool -t string -s /desktop/gnome/volume_manager/autoplay_dvd_command "vlc %m"
gconftool -t string -s /desktop/gnome/url-handlers/mms "vlc %s"
gconftool -t string -s /desktop/gnome/url-handlers/mmsh "vlc %s"
gconftool -t string -s /desktop/gnome/url-handlers/rtsp "vlc %s"
gconftool -t string -s /desktop/gnome/url-handlers/rtp "vlc %s"

