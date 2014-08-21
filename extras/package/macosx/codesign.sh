#!/bin/bash
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
   -t            Entitlements file to use
   -g            Enable additional magic
EOF

}

while getopts "hi:t:g" OPTION
do
     case $OPTION in
         h)
             usage
             exit 1
         ;;
         i)
             IDENTITY=$OPTARG
         ;;
         t)
             OPTIONS="--entitlements $OPTARG"
         ;;
         g)
             GK="yes"
         ;;
     esac
done
shift $(($OPTIND - 1))

if [ "x$1" != "x" ]; then
    usage
    exit 1
fi

if test -z "$GK"
then

    info "Signing frameworks"
    find VLC.app/Contents/Frameworks/* -type f -exec codesign --force -s "$IDENTITY" $OPTIONS '{}' \;

    info "Signing the executable"
    codesign --force -s "$IDENTITY" $OPTIONS VLC.app/Contents/MacOS/VLC

    info "Signing the modules"
    find VLC.app/Contents/MacOS/plugins/* -type f -exec codesign --force -s "$IDENTITY" $OPTIONS '{}' \;

    info "Signing the libraries"
    find VLC.app/Contents/MacOS/lib/* -type f -exec codesign --force -s "$IDENTITY" $OPTIONS '{}' \;

    info "Signing the lua stuff"
    find VLC.app/Contents/MacOS/share/lua/* -name *luac -type f -exec codesign --force -s "$IDENTITY" $OPTIONS '{}' \;
else
    IDENTIFIER="com.binarymethod.BGHUDAppKit"

    FIRSTPARTOF_REQUIREMENT="=designated => anchor apple generic  and identifier \""
    SECONDPARTOF_REQUIREMENT="\" and ((cert leaf[field.1.2.840.113635.100.6.1.9] exists) or ( certificate 1[field.1.2.840.113635.100.6.2.6] exists and certificate leaf[field.1.2.840.113635.100.6.1.13] exists  and certificate leaf[subject.OU] = \"75GAHG3SZQ\" ))"

    info "Cleaning frameworks"
    find VLC.app/Contents/Frameworks -type f -name ".DS_Store" -exec rm '{}' \;
    find VLC.app/Contents/Frameworks -type f -name "*.textile" -exec rm '{}' \;
    find VLC.app/Contents/Frameworks -type f -name "*.txt" -exec rm '{}' \;

    info "Signing frameworks"
    codesign --force --deep --verbose -s "$IDENTITY" --preserve-metadata=identifier,entitlements,resource-rules --requirements "$FIRSTPARTOF_REQUIREMENT$IDENTIFIER$SECONDPARTOF_REQUIREMENT" VLC.app/Contents/Frameworks/BGHUDAppKit.framework/Versions/A
    IDENTIFIER="com.growl.growlframework"
    codesign --force --deep --verbose -s "$IDENTITY" --preserve-metadata=identifier,entitlements,resource-rules --requirements "$FIRSTPARTOF_REQUIREMENT$IDENTIFIER$SECONDPARTOF_REQUIREMENT" VLC.app/Contents/Frameworks/Growl.framework/Versions/A
    IDENTIFIER="org.andymatuschak.Sparkle"
    codesign --force --deep --verbose -s "$IDENTITY" --preserve-metadata=identifier,entitlements,resource-rules --requirements "$FIRSTPARTOF_REQUIREMENT$IDENTIFIER$SECONDPARTOF_REQUIREMENT" VLC.app/Contents/Frameworks/Sparkle.framework/Versions/A

    info "Signing the framework headers"
    for i in `find VLC.app/Contents/Frameworks/* -type f -name "*.h" -exec echo {} \;`
    do
        fbname=$(basename "$i")
        filename="${fbname%.*}"

        codesign --force -s "$IDENTITY" --preserve-metadata=identifier,entitlements,resource-rules --requirements "$FIRSTPARTOF_REQUIREMENT$filename$SECONDPARTOF_REQUIREMENT" $i
    done

    info "Signing the framework strings"
    for i in `find VLC.app/Contents/Frameworks/* -type f -name "*.strings" -exec echo {} \;`
    do
        fbname=$(basename "$i")
        filename="${fbname%.*}"

        codesign --force -s "$IDENTITY" --preserve-metadata=identifier,entitlements,resource-rules --requirements "$FIRSTPARTOF_REQUIREMENT$filename$SECONDPARTOF_REQUIREMENT" $i
    done

    info "Signing the framework plist files"
    for i in `find VLC.app/Contents/Frameworks/* -type f -name "*.plist" -exec echo {} \;`
    do
        fbname=$(basename "$i")
        filename="${fbname%.*}"

        codesign --force -s "$IDENTITY" --preserve-metadata=identifier,entitlements,resource-rules --requirements "$FIRSTPARTOF_REQUIREMENT$filename$SECONDPARTOF_REQUIREMENT" $i
    done

    info "Signing the framework nib files"
    for i in `find VLC.app/Contents/Frameworks/* -type f -name "*.nib" -exec echo {} \;`
    do
        fbname=$(basename "$i")
        filename="${fbname%.*}"

        codesign --force -s "$IDENTITY" --preserve-metadata=identifier,entitlements,resource-rules --requirements "$FIRSTPARTOF_REQUIREMENT$filename$SECONDPARTOF_REQUIREMENT" $i
    done

    info "Signing the Sparkle updater tool"
    for i in `find VLC.app/Contents/Frameworks/Sparkle.framework/Versions/A/Resources -type f -name "PkgInfo" -exec echo {} \;`
    do
        fbname=$(basename "$i")
        filename="${fbname%.*}"

        codesign --force -s "$IDENTITY" --preserve-metadata=identifier,entitlements,resource-rules --requirements "$FIRSTPARTOF_REQUIREMENT$filename$SECONDPARTOF_REQUIREMENT" $i
    done
    for i in `find VLC.app/Contents/Frameworks/Sparkle.framework/Versions/A/Resources -type f -name "Autoupdate" -exec echo {} \;`
    do
        codesign --force -s "$IDENTITY" --preserve-metadata=identifier,entitlements,resource-rules --requirements "$FIRSTPARTOF_REQUIREMENT$filename$SECONDPARTOF_REQUIREMENT" $i
    done
    for i in `find VLC.app/Contents/Frameworks/Sparkle.framework/Versions/A/Resources -type f -name "*.icns" -exec echo {} \;`
    do
        fbname=$(basename "$i")
        filename="${fbname%.*}"

        codesign --force -s "$IDENTITY" --preserve-metadata=identifier,entitlements,resource-rules --requirements "$FIRSTPARTOF_REQUIREMENT$filename$SECONDPARTOF_REQUIREMENT" $i
    done

    info "Signing the headers"
    for i in `find VLC.app/Contents/MacOS/include/* -type f -exec echo {} \;`
    do
        fbname=$(basename "$i")
        filename="${fbname%.*}"

        codesign --force -s "$IDENTITY" --preserve-metadata=identifier,entitlements,resource-rules --requirements "$FIRSTPARTOF_REQUIREMENT$filename$SECONDPARTOF_REQUIREMENT" $i
    done

    info "Signing the modules"

    for i in `find VLC.app/Contents/MacOS/plugins/* -type f -exec echo {} \;`
    do
        fbname=$(basename "$i")
        filename="${fbname%.*}"

        codesign --force -s "$IDENTITY" --preserve-metadata=identifier,entitlements,resource-rules --requirements "$FIRSTPARTOF_REQUIREMENT$filename$SECONDPARTOF_REQUIREMENT" $i
    done

    info "Signing the libraries"

    for i in `find VLC.app/Contents/MacOS/lib/* -type f -exec echo {} \;`
    do
        fbname=$(basename "$i")
        filename="${fbname%.*}"

        codesign --force -s "$IDENTITY" --preserve-metadata=identifier,entitlements,resource-rules --requirements "$FIRSTPARTOF_REQUIREMENT$filename$SECONDPARTOF_REQUIREMENT" $i
    done

    info "Signing share"

    for i in `find VLC.app/Contents/MacOS/share/* -type f -exec echo {} \;`
    do
        fbname=$(basename "$i")
        filename="${fbname%.*}"

        codesign --force -s "$IDENTITY" --preserve-metadata=identifier,entitlements,resource-rules --requirements "$FIRSTPARTOF_REQUIREMENT$filename$SECONDPARTOF_REQUIREMENT" $i
    done

    info "Signing the executable"
    codesign --force -s "$IDENTITY" --requirements "$FIRSTPARTOF_REQUIREMENTorg.videolan.vlc$SECONDPARTOF_REQUIREMENT" VLC.app/Contents/MacOS/VLC
fi

info "all items signed, validating..."

info "Validating binary"
codesign --verify --verbose=4 VLC.app/Contents/MacOS/VLC

info "Validating frameworks"
find VLC.app/Contents/Frameworks/* -type f -exec codesign --verify '{}' \;

info "Validating modules"
find VLC.app/Contents/MacOS/plugins/* -type f -exec codesign --verify '{}' \;

info "Validating libraries"
find VLC.app/Contents/MacOS/lib/* -type f -exec codesign --verify '{}' \;

info "Validating lua stuff"
find VLC.app/Contents/MacOS/share/lua/* -name *luac -type f -exec codesign --verify '{}' \;

info "Validation complete"
