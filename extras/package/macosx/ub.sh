#!/bin/bash
set -e

# This script allows you to create a Universal Binary of VLC.app. It requires
# pre-compiled binaries for Intel-, Intel64- and PowerPC-based Macs; no sources.
# PPC64 is not supported right now, but could be added with little effort.
# Using different OS X SDKs for both platforms is absolutely fine of course.
# All you need to do is CHANGE THE ROOTS and READ THE COMMENTS.
# Happy releasing!

# INTELROOT == path to VLC.app compiled on an Intel-based Mac (e.g. jones).
# INTEL64ROOT == path to VLC.app compiled in 64bit mode (e.g. on jones with -m64).
# PPCROOT   == path to VLC.app compiled on a PowerPC-based Mac (e.g. veda).
# PPCROOT=/Volumes/vlc-1.0.2/VLC.app
# Note that these roots only require read-access and won't be changed at all.

# UBROOT    == path to a VLC.app bundle which will contain the Universal Binary.

SRCROOT=`dirname $0`/../../..
WD=`pwd`
cd $SRCROOT
SRCROOT=`pwd`
cd $WD

#############################################
# Config

INTELROOT=$SRCROOT/32bit/VLC.app
INTEL64ROOT=$SRCROOT/64bit/VLC.app
# PPCROOT
UBROOT=$SRCROOT/VLC.app

#
#############################################

echo "Creating VLC in $UBROOT"
rm -Rf $UBROOT
cp -Rf $INTEL64ROOT $UBROOT

LIBS=Contents/MacOS/lib
PLUGINS=Contents/MacOS/plugins
FRAMEWORKS=Contents/Frameworks
rm -Rf $SRCROOT/VLC.app/$LIBS/*
rm -Rf $SRCROOT/VLC.app/Contents/MacOS/VLC
rm -Rf $SRCROOT/VLC.app/$PLUGINS/*
rm -Rf $SRCROOT/VLC.app/$FRAMEWORKS/Growl.framework/Versions/A/Growl
rm -Rf $SRCROOT/VLC.app/$FRAMEWORKS/BGHUDAppKit.framework/BGHUDAppKit
rm -Rf $SRCROOT/VLC.app/$FRAMEWORKS/BGHUDAppKit.framework/Versions/A/BGHUDAppKit
rm -Rf $SRCROOT/VLC.app/$FRAMEWORKS/Sparkle.framework/Versions/A/Sparkle
rm -Rf $SRCROOT/VLC.app/$FRAMEWORKS/Sparkle.framework/Resources/relaunch

function do_lipo {
    file="$1"
    files=""
    echo "..."$file
    if [ "x$PPCROOT" != "x" ]; then
        if [ -e "$PPCROOT/$file" ]; then
            files="$PPCROOT/$file $files"
        fi
    fi
    if [ "x$INTELROOT" != "x" ]; then
        if [ -e "$INTELROOT/$file" ]; then
            files="$INTELROOT/$file $files"
        fi
    fi
    if [ "x$INTEL64ROOT" != "x" ]; then
        if [ -e "$INTEL64ROOT/$file" ]; then
            files="$INTEL64ROOT/$file $files"
        fi
    fi
    if [ "x$files" != "x" ]; then
        lipo $files -create -output $UBROOT/$file
    fi;
}

echo "Installing libs"
for i in `ls $INTELROOT/$LIBS/ | grep .dylib`
do
    do_lipo $LIBS/$i
done

echo "Installing modules"
for i in `ls $INTELROOT/$PLUGINS/ | grep .dylib`
do
    do_lipo $PLUGINS/$i
done

echo "Installing frameworks"
do_lipo $FRAMEWORKS/Growl.framework/Versions/A/Growl
do_lipo $FRAMEWORKS/BGHUDAppKit.framework/BGHUDAppKit
do_lipo $FRAMEWORKS/BGHUDAppKit.framework/Versions/A/BGHUDAppKit
do_lipo $FRAMEWORKS/Sparkle.framework/Versions/A/Sparkle
do_lipo $FRAMEWORKS/Sparkle.framework/Resources/relaunch

echo "Installing VLC"
do_lipo Contents/MacOS/VLC

echo "Installing Extra modules"

# The following fixes modules, which aren't available on all platforms
do_lipo $LIBS/libSDL_image.0.dylib
do_lipo $LIBS/libtiff.3.dylib
do_lipo $LIBS/libtiff.3.dylib
do_lipo $PLUGINS/libsdl_image_plugin.dylib
do_lipo $PLUGINS/libquartztext_plugin.dylib
do_lipo $PLUGINS/libgoom_plugin.dylib
if [ "x$INTELROOT" != "x" ]; then
    cp $INTELROOT/$PLUGINS/*mmx* $UBROOT/$PLUGINS/
    cp $INTELROOT/$PLUGINS/*3dn* $UBROOT/$PLUGINS/
fi
if [ "x$INTEL64ROOT" != "x" ]; then
    cp $INTEL64ROOT/$PLUGINS/*sse* $UBROOT/$PLUGINS/
fi
if [ "x$PPCROOT" != "x" ]; then
    cp $PPCROOT/Contents/MacOS/modules/*altivec* $UBROOT/Contents/MacOS/modules/
    cp $PPCROOT/Contents/MacOS/lib/libvlc.dylib $UBROOT/Contents/MacOS/lib/
    cp $PPCROOT/Contents/MacOS/lib/libvlccore.dylib $UBROOT/Contents/MacOS/lib/
fi


echo "Copying plugins.dat"
set +x

if [ "x$PPCROOT" != "x" ]; then
    cp $PPCROOT/$PLUGINS/plugins-*.dat $UBROOT/$PLUGINS/
fi
if [ "x$INTELROOT" != "x" ]; then
    cp $INTELROOT/$PLUGINS/plugins-*.dat $UBROOT/$PLUGINS/
fi
if [ "x$INTEL64ROOT" != "x" ]; then
    cp $INTEL64ROOT/$PLUGINS/plugins-*.dat $UBROOT/$PLUGINS/
fi


# Now, you need to copy the resulting UBROOT bundle into VLC's build directory
# and make sure it is named "VLC.app".
# Afterwards, run 'make package-macosx' and follow release_howto.txt in /doc
