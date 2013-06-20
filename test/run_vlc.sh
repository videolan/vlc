#! /bin/sh

set -e

VLC="./vlc -I dummy --ignore-config"

$VLC -H
$VLC -vv vlc://quit
$VLC -vv --play-and-exit vlc://nop
