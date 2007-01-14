#!/bin/bash

INTELROOT=/Volumes/vlc-0.8.6a/VLC.app
PPCROOT=/Applications/VLC.app
UBROOT=/Users/fpk/0.8.6a/VLC-release.app

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
