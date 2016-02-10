#! /bin/sh

set -e

VLC="./vlc --ignore-config"

$VLC -H
$VLC -Idummy vlc://quit
$VLC -vv -Irc vlc://quit
$VLC -vv -Irc --play-and-exit vlc://nop

LSAN_OPTIONS=exitcode=0
export LSAN_OPTIONS

$VLC --play-and-exit vlc://nop
