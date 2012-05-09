#! /bin/sh

VLC="./vlc --ignore-config --no-one-instance-when-started-from-file"

$VLC -vv vlc://quit
$VLC -vv --play-and-exit vlc://nop
