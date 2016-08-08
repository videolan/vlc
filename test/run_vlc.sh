#! /bin/sh

set -e

VLC="./vlc --ignore-config --rc-fake-tty"

$VLC -H
$VLC -Idummy vlc://quit
$VLC -vv -Irc,oldrc vlc://quit
$VLC -vv -Irc,oldrc --play-and-exit vlc://nop

LSAN_OPTIONS=exitcode=0
export LSAN_OPTIONS

$VLC --play-and-exit vlc://nop
