#! /bin/sh

set -e

VLC="./vlc --ignore-config --rc-fake-tty"

$VLC -H
$VLC -Idummy vlc://quit
$VLC -vv -Irc,oldrc vlc://quit
$VLC -vv -Irc,oldrc --play-and-exit vlc://nop

ASAN_OPTIONS="$ASAN_OPTIONS,detect_leaks=0"
export ASAN_OPTIONS

$VLC --play-and-exit vlc://nop
