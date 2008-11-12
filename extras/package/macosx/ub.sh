#!/bin/bash

# This script allows you to create a Universal Binary of VLC.app. It requires 
# pre-compiled binaries for Intel- and PowerPC-based Macs; no sources.
# Exotic sub-platforms aka x86_64 or even ppc64 are not supported right now.
# Using different OS X SDKs for both platforms is absolutely fine of course.
# All you need to do is CHANGE THE ROOTS and READ THE COMMENTS.
# Happy releasing!

# INTELROOT == path to VLC.app compiled on an Intel-based Mac (e.g. jones).
INTELROOT=/Applications/VLC.app
# PPCROOT   == path to VLC.app compiled on a PowerPC-based Mac (e.g. veda).
PPCROOT=/Volumes/vlc-0.9.0-test3/VLC.app
# Note that these 2 roots only require read-access and won't be changed at all.

# UBROOT    == path to a VLC.app bundle which will contain the Universal Binary.
# Note that you should empty the following folders: lib, modules
# and remove the VLC binary in MacOS (just for sanity reasons)
UBROOT=/Users/fpk/VLC-release.app

for i in `ls $INTELROOT/Contents/MacOS/lib/`
do
        echo $i
        lipo $INTELROOT/Contents/MacOS/lib/$i $PPCROOT/Contents/MacOS/lib/$i -create -output $UBROOT/Contents/MacOS/lib/$i
done
for i in `ls $INTELROOT/Contents/MacOS/modules/`
do
        echo $i
        lipo $INTELROOT/Contents/MacOS/modules/$i $PPCROOT/Contents/MacOS/modules/$i -create -output $UBROOT/Contents/MacOS/modules/$i
done
lipo $INTELROOT/Contents/MacOS/VLC $PPCROOT/Contents/MacOS/VLC -create -output $UBROOT/Contents/MacOS/VLC
cp $INTELROOT/Contents/MacOS/modules/*mmx* $UBROOT/Contents/MacOS/modules/
cp $INTELROOT/Contents/MacOS/modules/*sse* $UBROOT/Contents/MacOS/modules/
cp $INTELROOT/Contents/MacOS/modules/*3dn* $UBROOT/Contents/MacOS/modules/
cp $PPCROOT/Contents/MacOS/modules/*altivec* $UBROOT/Contents/MacOS/modules/

# Now, you need to copy the resulting UBROOT folder into VLC's build directory 
# and make sure it is actually named "VLC-release.app".
# Afterwards, run 'make package-macosx' and follow release_howto.txt in /doc
