#!/usr/bin/env zsh
# Helper script to install zsh completion for VLC media player
# © 2008 Rafaël Carré <funman@videolanorg>


HOST=`gcc -dumpmachine`
case $HOST in
    *darwin*)
        SUFFIX=dylib
    ;;
    *cygwin*|*mingw*)
        SUFFIX=dll
    ;;
    *linux*|*bsd*)
        SUFFIX=so
    ;;
    *)
        echo "WARNING: Unknown platform: \'$HOST\', can't check for libraries"
    ;;
esac

#Distributors can run BUILDDIR=XXX ./zsh_completion.sh
[ -z "$BUILDDIR" ] && BUILDDIR=../../

VLC_PLUGIN_PATH="$BUILDDIR"
export VLC_PLUGIN_PATH

function find_libvlc {
    [ -z "$SUFFIX" ] && return 0 # linking will fail if lib isn't found
    for i in $BUILDDIR/lib/.libs/libvlc.$SUFFIX $BUILDDIR/lib/libvlc.$SUFFIX; do
        [ -e $i ] && LIBVLC=$i && return 0
    done
    return 1
}

function find_libvlccore {
    [ -z "$SUFFIX" ] && return 0 # linking will fail if lib isn't found
    for i in $BUILDDIR/src/.libs/libvlccore.$SUFFIX $BUILDDIR/src/libvlccore.$SUFFIX; do
        [ -e $i ] && LIBVLCCORE=$i && return 0
    done
    return 1
}

while [ -z "$LIBVLC" ]; do
    if ! find_libvlc; then
        printf "Please enter the directory where you built vlc: "
        read BUILDDIR
    fi
done

if ! find_libvlccore; then
    echo "libvlccore not found !"
    exit 1
fi

export LD_LIBRARY_PATH=$BUILDDIR/src/.libs
CXXFLAGS="$CXXFLAGS -g -O0"

if [ -e ../../extras/contrib/config.mak -a ! "`grep HOST ../../extras/contrib/config.mak 2>/dev/null|awk '{print $3}'`" != "$HOST" ]; then
    CXXFLAGS="-I../../extras/contrib/include"
fi

[ -z "$CXX" ] && CXX=g++

ZSH_BUILD="$CXX $CXXFLAGS -DHAVE_CONFIG_H -I$BUILDDIR -I$BUILDDIR/include -I../../include zsh.cpp $LIBVLC $LIBVLCCORE -o zsh_gen"

echo $ZSH_BUILD
echo
eval $ZSH_BUILD || exit 1

printf "Generating zsh completion in _vlc ... "

VLC_PLUGIN_PATH=$BUILDDIR/modules
if ! ./zsh_gen >_vlc 2>/dev/null; then
    echo "
ERROR: the generation failed.... :(
Please press enter to verify that all the VLC modules are shown"
    read i
    ./zsh_gen -vv --list
    echo "
If they are shown, press enter to see if you can debug the problem
It will be reproduced by running \"./zsh_gen -vv\""
    read i
    ./zsh_gen -vv
    exit 1
fi

echo "done"

ZSH_FPATH=`echo $fpath|cut -d\  -f1`
[ -z "$ZSH_FPATH" ] && exit 0 # don't know where to install

echo "
You should now copy _vlc to $ZSH_FPATH and then
remove ~/.zcompdump and restart your running zsh instances,
or run \"compinit\" to start using vlc completion immediately."
