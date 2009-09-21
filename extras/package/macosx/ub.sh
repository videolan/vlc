#!/bin/bash

# This script allows you to create a Universal Binary of VLC.app. It requires 
# pre-compiled binaries for Intel-, Intel64- and PowerPC-based Macs; no sources.
# PPC64 is not supported right now, but could be added with little effort.
# Using different OS X SDKs for both platforms is absolutely fine of course.
# All you need to do is CHANGE THE ROOTS and READ THE COMMENTS.
# Happy releasing!

# INTELROOT == path to VLC.app compiled on an Intel-based Mac (e.g. jones).
# INTEL64ROOT == path to VLC.app compiled in 64bit mode (e.g. on jones with -m64).
INTELROOT=/Applications/VLC.app
INTEL64ROOT=/Users/fpk/VLC-release.app
# PPCROOT   == path to VLC.app compiled on a PowerPC-based Mac (e.g. veda).
PPCROOT=/Volumes/vlc-1.0.2/VLC.app
# Note that these roots only require read-access and won't be changed at all.

# UBROOT    == path to a VLC.app bundle which will contain the Universal Binary.
# Note that you should empty the following folders: lib, modules
# and remove the VLC binary in MacOS
UBROOT=/Users/fpk/VLC.app

for i in `ls $INTELROOT/Contents/MacOS/lib/`
do
        echo $i
        lipo $INTELROOT/Contents/MacOS/lib/$i $INTEL64ROOT/Contents/MacOS/lib/$i $PPCROOT/Contents/MacOS/lib/$i -create -output $UBROOT/Contents/MacOS/lib/$i
done
for i in `ls $INTELROOT/Contents/MacOS/modules/`
do
        echo $i
        lipo $INTELROOT/Contents/MacOS/modules/$i $INTEL64ROOT/Contents/MacOS/modules/$i $PPCROOT/Contents/MacOS/modules/$i -create -output $UBROOT/Contents/MacOS/modules/$i
done
lipo $INTELROOT/Contents/MacOS/VLC $INTEL64ROOT/Contents/MacOS/VLC $PPCROOT/Contents/MacOS/VLC -create -output $UBROOT/Contents/MacOS/VLC

# The following fixes modules, which aren't available on all platforms
lipo $INTELROOT/Contents/MacOS/lib/libSDL_image.0.dylib $PPCROOT/Contents/MacOS/lib/libSDL_image.0.dylib -create -output $UBROOT/Contents/MacOS/lib/libSDL_image.0.dylib
lipo $INTELROOT/Contents/MacOS/lib/libSDL-1.3.0.dylib $PPCROOT/Contents/MacOS/lib/libSDL-1.3.0.dylib -create -output $UBROOT/Contents/MacOS/lib/libSDL-1.3.0.dylib
lipo $INTELROOT/Contents/MacOS/lib/libjpeg.7.dylib $PPCROOT/Contents/MacOS/lib/libjpeg.7.dylib -create -output $UBROOT/Contents/MacOS/lib/libjpeg.7.dylib
lipo $INTELROOT/Contents/MacOS/lib/libtiff.3.dylib $PPCROOT/Contents/MacOS/lib/libtiff.3.dylib -create -output $UBROOT/Contents/MacOS/lib/libtiff.3.dylib
lipo $INTELROOT/Contents/MacOS/modules/libsdl_image_plugin.dylib $PPCROOT/Contents/MacOS/modules/libsdl_image_plugin.dylib -create -output $UBROOT/Contents/MacOS/modules/libsdl_image_plugin.dylib
lipo $INTELROOT/Contents/MacOS/modules/libquartztext_plugin.dylib $PPCROOT/Contents/MacOS/modules/libquartztext_plugin.dylib -create -output $UBROOT/Contents/MacOS/modules/libquartztext_plugin.dylib
lipo $INTELROOT/Contents/MacOS/modules/libgoom_plugin.dylib $PPCROOT/Contents/MacOS/modules/libgoom_plugin.dylib -create -output $UBROOT/Contents/MacOS/modules/libgoom_plugin.dylib
cp $INTELROOT/Contents/MacOS/modules/*mmx* $UBROOT/Contents/MacOS/modules/
cp $INTELROOT/Contents/MacOS/modules/*sse* $UBROOT/Contents/MacOS/modules/
cp $INTELROOT/Contents/MacOS/modules/*3dn* $UBROOT/Contents/MacOS/modules/
cp $PPCROOT/Contents/MacOS/modules/*altivec* $UBROOT/Contents/MacOS/modules/
cp $PPCROOT/Contents/MacOS/lib/libvlc.dylib $UBROOT/Contents/MacOS/lib/
cp $PPCROOT/Contents/MacOS/lib/libvlccore.dylib $UBROOT/Contents/MacOS/lib/

# Now, you need to copy the resulting UBROOT bundle into VLC's build directory 
# and make sure it is named "VLC-release.app".
# Afterwards, run 'make package-macosx' and follow release_howto.txt in /doc
