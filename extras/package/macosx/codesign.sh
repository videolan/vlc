#!/bin/bash
# Copyright (C) 2012-2017 VLC authors and VideoLAN
# Copyright (C) 2012-2014 Felix Paul Kühne <fkuehne at videolan dot org>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.

set -e
# set -o xtrace

info()
{
    green='\x1B[1;32m'
    normal='\x1B[0m'
    echo -e "[${green}codesign${normal}] $1"
}

usage()
{
cat << EOF
usage: $0 [options]

Sign VLC.app in the current directory

OPTIONS:
   -h            Show this help
   -i            Identity to use
   -g            Use gatekeeper mode (additional magic)
EOF

}

while getopts "hi:g" OPTION
do
     case $OPTION in
         h)
             usage
             exit 1
         ;;
         i)
             IDENTITY=$OPTARG
         ;;
         g)
             GK="yes"
         ;;
         *)
             usage
             exit 1
         ;;
     esac
done
shift $(($OPTIND - 1))

if [ "x$1" != "x" ]; then
    usage
    exit 1
fi

VLCCACHEGEN=""
if [ -e "./bin/vlc-cache-gen" ]; then
    VLCCACHEGEN="./bin/vlc-cache-gen"
fi
if [ -e "./vlc-cache-gen" ]; then
    VLCCACHEGEN="./vlc-cache-gen"
fi

if [ -z "$VLCCACHEGEN" ]; then
info "WARN: Cannot find vlc-cache-gen, cache will be corrupt after signing"
fi

# Call with $1 = file or folder, $2 = identifier (if empty, file name is used)
sign()
{
    # info "Signing file $1 with identifier $IDENTIFIER"

    codesign --force --verbose -s "$IDENTITY" "$1"
}


info "Cleaning frameworks"
find VLC.app/Contents/Frameworks -type f -name ".DS_Store" -exec rm '{}' \;
find VLC.app/Contents/Frameworks -type f -name "*.textile" -exec rm '{}' \;
find VLC.app/Contents/Frameworks -type f -name "*.txt" -exec rm '{}' \;

info "Signing frameworks"

sign "VLC.app/Contents/Frameworks/Growl.framework/Versions/A" "com.growl.growlframework"

sign "VLC.app/Contents/Frameworks/Sparkle.framework/Versions/A/Resources/Autoupdate.app/Contents/MacOS/fileop"
sign "VLC.app/Contents/Frameworks/Sparkle.framework/Resources/Autoupdate.app" "org.sparkle-project.Sparkle.Autoupdate"
sign "VLC.app/Contents/Frameworks/Sparkle.framework/Versions/A" "org.sparkle-project.Sparkle"

if [ -e "VLC.app/Contents/Frameworks/Breakpad.framework" ]; then
    sign "VLC.app/Contents/Frameworks/Breakpad.framework/Resources/crash_report_sender.app" "com.Breakpad.crash_report_sender"
    sign "VLC.app/Contents/Frameworks/Breakpad.framework/Versions/A" "com.googlecode.google-breakpad"
fi

info "Signing the framework headers"
for i in $(find VLC.app/Contents/Frameworks -type f -name "*.h" -exec echo {} \;)
do
    sign "$i"
done

info "Signing the framework strings"
for i in $(find VLC.app/Contents/Frameworks -type f -name "*.strings" -exec echo {} \;)
do
    sign "$i"
done

info "Signing the framework plist files"
for i in $(find VLC.app/Contents/Frameworks -type f -name "*.plist" -exec echo {} \;)
do
    sign "$i"
done

info "Signing the framework nib files"
for i in $(find VLC.app/Contents/Frameworks -type f -name "*.nib" -exec echo {} \;)
do
    sign "$i"
done

info "Signing the headers"
for i in $(find VLC.app/Contents/MacOS/include -type f -exec echo {} \;)
do
    sign "$i"
done

info "Signing the modules"

for i in $(find VLC.app/Contents/MacOS/plugins -type f \( -name "*.dylib" -o -name "*.jar" \)  -exec echo {} \;)
do
    sign "$i"
done

if [ ! -z "$VLCCACHEGEN" ]; then
    $VLCCACHEGEN VLC.app/Contents/MacOS/plugins
fi

sign "VLC.app/Contents/MacOS/plugins/plugins.dat"

info "Signing the libraries"

for i in $(find VLC.app/Contents/MacOS/lib -type f -exec echo {} \;)
do
    sign "$i"
done

info "Signing share"

for i in $(find VLC.app/Contents/MacOS/share -type f -exec echo {} \;)
do
    sign "$i"
done

info "Signing the executable"
sign "VLC.app" "org.videolan.vlc"


info "all items signed, validating..."

info "Validating frameworks"
if [ -e "VLC.app/Contents/Frameworks/Breakpad.framework" ]; then
    codesign --verify -vv VLC.app/Contents/Frameworks/Breakpad.framework
fi

codesign --verify -vv VLC.app/Contents/Frameworks/Growl.framework
codesign --verify -vv VLC.app/Contents/Frameworks/Sparkle.framework

info "Validating autoupdate app"
codesign --verify -vv VLC.app/Contents/Frameworks/Sparkle.framework/Versions/Current/Resources/Autoupdate.app

info "Validating complete bundle"
codesign --verify --deep --strict --verbose=4 VLC.app

if [ ! -z "$GK" ]; then
    spctl -a -t exec -vv VLC.app
fi


info "Validation complete"
