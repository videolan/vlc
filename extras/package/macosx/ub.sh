#!/bin/bash

INTELROOT=/Applications/VLC.app
PPCROOT=/Volumes/vlc-0.9.0-test3/VLC.app
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
