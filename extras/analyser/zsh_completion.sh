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
        echo WARNING: Unknown platform: \'$HOST\'
    ;;
esac

if test -z "$SUFFIX"; then
    echo "ERROR: unknown suffix for shared objects
Please run \"SUFFIX=xxx $0\"
where xxx is the shared object extension on your platform."
    exit 1
fi

#Distributors can run BUILDDIR=XXX ./zsh_completion.sh
if test -z "$BUILDDIR"; then
    BUILDDIR=../../
fi

function find_libvlc {
    for i in $BUILDDIR/src/.libs/libvlc.$SUFFIX $BUILDDIR/src/libvlc.$SUFFIX; do
        test -e $i && LIBVLC=$i && return 0
    done
    return 1
}

function find_libvlccore {
    for i in $BUILDDIR/src/.libs/libvlccore.$SUFFIX $BUILDDIR/src/libvlccore.$SUFFIX; do
        test -e $i && LIBVLCCORE=$i && return 0
    done
    return 1
}

while test -z "$LIBVLC"; do
    if ! find_libvlc; then
        /bin/echo -n "Please enter the directory where you built vlc: "
        read BUILDDIR
    fi
done

echo "libvlc found !"

if ! find_libvlccore; then
    /bin/echo -n "libvlccore not found ! Linking will fail !"
fi

LD_LIBRARY_PATH=$BUILDDIR/src/.libs

if test -e ../../extras/contrib/config.mak -a ! "`grep HOST ../../extras/contrib/config.mak 2>/dev/null|awk '{print $3}'`" != "$HOST"; then
    echo "contribs found !"
    CPPFLAGS="-I../../extras/contrib/include"
fi

if test -z "$CXX"; then
    CXX=g++
fi

ZSH_BUILD="$CXX $CPPFLAGS $CXXFLAGS -D__LIBVLC__ -DHAVE_CONFIG_H -I$BUILDDIR -I$BUILDDIR/include -I../../include zsh.cpp $LIBVLC $LIBVLCCORE -o zsh_gen"

echo "Building zsh completion generator ...  "
echo $ZSH_BUILD
echo
eval $ZSH_BUILD || exit 1

echo "Generating zsh completion ..."
if ! ./zsh_gen --plugin-path=$BUILDDIR >_vlc 2>/dev/null; then
    echo "ERROR: the generation failed.... :(
Please press enter to verify that all the VLC modules are shown"
    read i
    ./zsh_gen --plugin-path=$BUILDDIR -vvv --list
    echo "
If they are shown, press enter to see if you can debug the problem
It will be reproduced by running \"./zsh_gen --plugin-path=$BUILDDIR -vvv\""
    read i
    ./zsh_gen --plugin-path=$BUILDDIR -vvv
    exit 1
fi

echo "zsh completion is `echo \`wc -l _vlc\`` lines long !"

test -z "$NOINSTALL" || exit 0
#Distributors can run NOINSTALL=mg ./zsh_completion.sh

if ! /usr/bin/which zsh >/dev/null 2>&1; then
    echo "ERROR: zsh not found, you'll have to copy the _vlc file manually"
    exit 1
fi

test -z "$ZSH_FPATH" && ZSH_FPATH=`zsh -c "echo \\$fpath|cut -d\" \" -f1"`
if test -z "$ZSH_FPATH"; then
    echo "ERROR: Could not find a directory where to install completion
Please run \"ZSH_FPATH=path $0\"
where path is the directory where you want to install completion"
    exit 1
fi

echo "completion will be installed in $ZSH_FPATH , using root privileges
Press Ctrl+C to abort installation, and copy _vlc manually"
read i
echo "Installing completion ..."
sudo sh -c "chown 0:0 _vlc && chmod 0644 _vlc && mv _vlc $ZSH_FPATH" || exit 1

echo "zsh completion for VLC successfully installed :)
Restart running zsh instances, or run \"compinit\" to start using it."
