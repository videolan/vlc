#!/bin/sh
APP=/media/developer/apps/usr/palm/applications/org.videolan.vlc.webos
export LD_LIBRARY_PATH="${APP}/vlc/lib:/usr/lib:/lib"
/lib/ld-linux-armhf.so.3 --list "${APP}/vlc/plugins/gui/libqt_plugin.so" 2>&1 | grep "not found"
