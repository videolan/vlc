#!/bin/sh
# $Id$
#
# Copyright © 2007 VideoLAN team
#
# Authors: http://forum.videolan.org/viewtopic.php?f=13&t=40560
#          Rafaël Carré <funman@videolanorg>
#
# Sets VLC media player the default application for video mime types
# on a freedesktop compliant desktop
#

MIME_FILE=~/.local/share/applications/defaults.list

if [ ! -f $MIME_FILE ]
    then echo "[Default Applications]" > $MIME_FILE
    else grep -v 'video/' $MIME_FILE > /tmp/vlc.defaults.list.tmp
    mv /tmp/vlc.defaults.list.tmp $MIME_FILE
fi

ls /usr/share/mime/video/* | sed -e 's@/usr/share/mime/@@' -e 's/\.xml/=vlc.desktop/' >> $MIME_FILE
