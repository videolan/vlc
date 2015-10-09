#!/bin/sh
set -e
set -x

info()
{
    local green="\033[1;32m"
    local normal="\033[0m"
    echo "[${green}build${normal}] $1"
}

ARCH="x86_64"
MINIMAL_OSX_VERSION="10.7"
OSX_VERSION=`xcrun --show-sdk-version`
SDKROOT=`xcode-select -print-path`/Platforms/MacOSX.platform/Developer/SDKs/MacOSX$OSX_VERSION.sdk

usage()
{
cat << EOF
usage: $0 [options]

Build vlc in the current directory

OPTIONS:
   -h            Show some help
   -q            Be quiet
   -r            Rebuild everything (tools, contribs, vlc)
   -k <sdk>      Use the specified sdk (default: $SDKROOT)
   -a <arch>     Use the specified arch (default: $ARCH)
EOF

}

spushd()
{
    pushd "$1" > /dev/null
}

spopd()
{
    popd > /dev/null
}

while getopts "hvrk:a:" OPTION
do
     case $OPTION in
         h)
             usage
             exit 1
             ;;
         q)
             set +x
             QUIET="yes"
         ;;
         r)
             REBUILD="yes"
         ;;
         a)
             ARCH=$OPTARG
         ;;
         k)
             SDKROOT=$OPTARG
         ;;
     esac
done
shift $(($OPTIND - 1))

if [ "x$1" != "x" ]; then
    usage
    exit 1
fi

#
# Various initialization
#

out="/dev/stdout"
if [ "$QUIET" = "yes" ]; then
    out="/dev/null"
fi

info "Building VLC for the Mac OS X"

spushd `dirname $0`/../../..
vlcroot=`pwd`
spopd

builddir=`pwd`

info "Building in \"$builddir\""

TRIPLET=$ARCH-apple-darwin11

export CC="xcrun clang"
export CXX="xcrun clang++"
export OBJC="xcrun clang"
export OSX_VERSION
export SDKROOT
export PATH="${vlcroot}/extras/tools/build/bin:/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:${vlcroot}/contrib/${TRIPLET}/bin:${PATH}"

#
# vlc/extras/tools
#

info "Building building tools"
spushd "${vlcroot}/extras/tools"
./bootstrap > $out
if [ "$REBUILD" = "yes" ]; then
    make clean
fi
make > $out
spopd


#
# vlc/contribs
#

info "Building contribs"
spushd "${vlcroot}/contrib"
mkdir -p contrib-$TRIPLET && cd contrib-$TRIPLET
../bootstrap --build=$TRIPLET --host=$TRIPLET > $out
if [ "$REBUILD" = "yes" ]; then
    make clean
fi
if [ ! -e "../$TRIPLET" ]; then
    make prebuilt > $out
fi
spopd


#
# vlc/bootstrap
#

info "Bootstrap-ing configure"
spushd "${vlcroot}"
if ! [ -e "${vlcroot}/configure" ]; then
    ${vlcroot}/bootstrap > $out
fi
spopd


#
# vlc/configure
#

if [ "${vlcroot}/configure" -nt Makefile ]; then

  ${vlcroot}/extras/package/macosx/configure.sh \
      --build=$TRIPLET \
      --host=$TRIPLET \
      --with-macosx-version-min=$MINIMAL_OSX_VERSION \
      --with-macosx-sdk=$SDKROOT > $out
fi


#
# make
#

core_count=`sysctl -n machdep.cpu.core_count`
let jobs=$core_count+1

if [ "$REBUILD" = "yes" ]; then
    info "Running make clean"
    make clean
fi

info "Running make -j$jobs"
make -j$jobs
