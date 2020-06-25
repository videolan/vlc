#!/bin/sh
set -e

info()
{
    local green="\033[1;32m"
    local normal="\033[0m"
    echo "[${green}build${normal}] $1"
}

SCRIPTDIR=$(dirname "$0")
source "$SCRIPTDIR/env.build.sh" "none"

SDKROOT=$(xcrun --show-sdk-path)
VLCBUILDDIR=""

CORE_COUNT=`getconf NPROCESSORS_ONLN 2>&1`
let JOBS=$CORE_COUNT+1


usage()
{
cat << EOF
usage: $0 [options]

Build vlc in the current directory

OPTIONS:
   -h            Show some help
   -q            Be quiet
   -j            Force number of cores to be used
   -r            Rebuild everything (tools, contribs, vlc)
   -c            Recompile contribs from sources
   -p            Build packages for all artifacts
   -i <n|u>      Create an installable package (n: nightly, u: unsigned stripped release archive)
   -k <sdk>      Use the specified sdk (default: $SDKROOT)
   -a <arch>     Use the specified arch (default: $ARCH)
   -C            Use the specified VLC build dir
   -b <url>      Enable breakpad support and send crash reports to this URL
   -d            Disable debug mode (on by default)
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

while getopts "qhvrcdpi:k:a:j:C:b:" OPTION
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
         d)
             NODEBUG="yes"
         ;;
         r)
             REBUILD="yes"
         ;;
         c)
             CONTRIBFROMSOURCE="yes"
         ;;
         p)
             PACKAGE="yes"
         ;;
         i)
             PACKAGETYPE=$OPTARG
         ;;
         a)
             ARCH=$OPTARG
         ;;
         k)
             SDKROOT=$OPTARG
         ;;
         j)
             JOBS=$OPTARG
         ;;
         C)
             VLCBUILDDIR=$OPTARG
         ;;
         b)
             BREAKPAD=$OPTARG
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

#
# Various initialization
#

out="/dev/stdout"
if [ "$QUIET" = "yes" ]; then
    out="/dev/null"
fi

info "Building VLC for the Mac OS X"

TRIPLET=$(vlcGetTriplet)
export SDKROOT
vlcSetBaseEnvironment
vlcroot="$(vlcGetRootDir)"

# Checking prerequisites
info "Checking for python3 ..."
python3 --version || { echo "python3 not found. Please install from python.org, or set" \
	"VLC_PATH environment variable to include python3." \
	; exit 1; }


builddir="$(pwd)"
info "Building in \"$builddir\""

#
# vlc/extras/tools
#

info "Building building tools"
spushd "${vlcroot}/extras/tools"
./bootstrap > $out
if [ "$REBUILD" = "yes" ]; then
    make clean
    ./bootstrap > $out
fi
make > $out
spopd

#
# vlc/contribs
#

vlcSetSymbolEnvironment
vlcSetContribEnvironment "$MINIMAL_OSX_VERSION"

info "Building contribs"
spushd "${vlcroot}/contrib"

if [ "$REBUILD" = "yes" ]; then
    rm -rf contrib-$TRIPLET
    rm -rf $TRIPLET
fi
mkdir -p contrib-$TRIPLET && cd contrib-$TRIPLET
../bootstrap --build=$TRIPLET --host=$TRIPLET > $out

if [ "$CONTRIBFROMSOURCE" = "yes" ]; then
    make list
    make fetch
    make -j$JOBS .gettext
    make -j$JOBS -k || make -j1

    if [ "$PACKAGE" = "yes" ]; then
        make package
    fi

else
if [ ! -e "../$TRIPLET" ]; then
    make prebuilt > $out
fi
fi
spopd


vlcUnsetContribEnvironment

#
# vlc/bootstrap
#

info "Bootstrap-ing configure"
spushd "${vlcroot}"
if ! [ -e "${vlcroot}/configure" ]; then
    ${vlcroot}/bootstrap > $out
fi
spopd


if [ ! -z "$VLCBUILDDIR" ];then
    mkdir -p $VLCBUILDDIR
    pushd $VLCBUILDDIR
fi
#
# vlc/configure
#

CONFIGFLAGS=""
if [ ! -z "$BREAKPAD" ]; then
     CONFIGFLAGS="$CONFIGFLAGS --with-breakpad=$BREAKPAD"
fi
if [ "$NODEBUG" = "yes" ]; then
     CONFIGFLAGS="$CONFIGFLAGS --disable-debug"
fi

if [ "${vlcroot}/configure" -nt Makefile ]; then

  ${vlcroot}/extras/package/macosx/configure.sh \
      --build=$TRIPLET \
      --host=$TRIPLET \
      --with-macosx-version-min=$MINIMAL_OSX_VERSION \
      --with-macosx-sdk=$SDKROOT \
      $CONFIGFLAGS \
      $VLC_CONFIGURE_ARGS > $out
fi

#
# make
#

if [ "$REBUILD" = "yes" ]; then
    info "Running make clean"
    make clean
fi

info "Running make -j$JOBS"
make -j$JOBS

info "Preparing VLC.app"
make VLC.app


if [ "$PACKAGETYPE" = "u" ]; then
    info "Copying app with debug symbols into VLC-debug.app and stripping"
    rm -rf VLC-debug.app
    cp -Rp VLC.app VLC-debug.app

    # Workaround for breakpad symbol parsing:
    # Symbols must be uploaded for libvlc(core).dylib, not libvlc(core).x.dylib
    (cd VLC-debug.app/Contents/MacOS/lib/ && rm libvlccore.dylib && mv libvlccore.*.dylib libvlccore.dylib)
    (cd VLC-debug.app/Contents/MacOS/lib/ && rm libvlc.dylib && mv libvlc.*.dylib libvlc.dylib)


    find VLC.app/ -name "*.dylib" -exec strip -x {} \;
    find VLC.app/ -type f -name "VLC" -exec strip -x {} \;
    find VLC.app/ -type f -name "Sparkle" -exec strip -x {} \;
    find VLC.app/ -type f -name "Growl" -exec strip -x {} \;
    find VLC.app/ -type f -name "Breakpad" -exec strip -x {} \;

    bin/vlc-cache-gen VLC.app/Contents/MacOS/plugins

    info "Building VLC release archive"
    make package-macosx-release
    make package-macosx-sdk

    shasum -a 512 vlc-*-release.zip
    shasum -a 512 vlc-macos-sdk-*.tar.gz

elif [ "$PACKAGETYPE" = "n" -o "$PACKAGE" = "yes" ]; then
    info "Building VLC dmg package"
    make package-macosx
    make package-macosx-sdk
fi

if [ ! -z "$VLCBUILDDIR" ]; then
    popd
fi
