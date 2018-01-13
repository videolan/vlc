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
OSX_KERNELVERSION=`uname -r | cut -d. -f1`
SDKROOT=`xcode-select -print-path`/Platforms/MacOSX.platform/Developer/SDKs/MacOSX$OSX_VERSION.sdk
VLCBUILDDIR=""

CORE_COUNT=`getconf NPROCESSORS_ONLN 2>&1`
let JOBS=$CORE_COUNT+1

if [ ! -z "$VLC_FORCE_KERNELVERSION" ]; then
    OSX_KERNELVERSION="$VLC_FORCE_KERNELVERSION"
fi

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

while getopts "hvrcpi:k:a:j:C:b:" OPTION
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

spushd `dirname $0`/../../..
vlcroot=`pwd`
spopd

builddir=`pwd`

info "Building in \"$builddir\""

TRIPLET=$ARCH-apple-darwin$OSX_KERNELVERSION

export CC="`xcrun --find clang`"
export CXX="`xcrun --find clang++`"
export OBJC="`xcrun --find clang`"
export OSX_VERSION
export SDKROOT
export PATH="${vlcroot}/extras/tools/build/bin:${vlcroot}/contrib/${TRIPLET}/bin:${VLC_PATH}:/bin:/sbin:/usr/bin:/usr/sbin"

# Select avcodec flavor to compile contribs with
export USE_FFMPEG=1

# The following symbols do not exist on the minimal macOS version (10.7), so they are disabled
# here. This allows compilation also with newer macOS SDKs.
# Added symbols in 10.13
export ac_cv_func_open_wmemstream=no
export ac_cv_func_fmemopen=no
export ac_cv_func_open_memstream=no
export ac_cv_func_futimens=no
export ac_cv_func_utimensat=no

# Added symbols between 10.11 and 10.12
export ac_cv_func_basename_r=no
export ac_cv_func_clock_getres=no
export ac_cv_func_clock_gettime=no
export ac_cv_func_clock_settime=no
export ac_cv_func_dirname_r=no
export ac_cv_func_getentropy=no
export ac_cv_func_mkostemp=no
export ac_cv_func_mkostemps=no

# Added symbols between 10.7 and 10.11
export ac_cv_func_ffsll=no
export ac_cv_func_flsll=no
export ac_cv_func_fdopendir=no
export ac_cv_func_openat=no
export ac_cv_func_fstatat=no
export ac_cv_func_readlinkat=no

# libnetwork does not exist yet on 10.7 (used by libcddb)
export ac_cv_lib_network_connect=no

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

# Usually, VLCs contrib libraries do not support partial availability at runtime.
# Forcing those errors has two reasons:
# - Some custom configure scripts include the right header for testing availability.
#   Those configure checks fail (correctly) with those errors, and replacements are
#   enabled. (e.g. ffmpeg)
# - This will fail the build if a partially available symbol is added later on
#   in contribs and not mentioned in the list of symbols above.
export CFLAGS="-Werror=partial-availability"
export CXXFLAGS="-Werror=partial-availability"
export OBJCFLAGS="-Werror=partial-availability"

info "Building contribs"
spushd "${vlcroot}/contrib"
mkdir -p contrib-$TRIPLET && cd contrib-$TRIPLET
../bootstrap --build=$TRIPLET --host=$TRIPLET > $out
if [ "$REBUILD" = "yes" ]; then
    make clean
fi
if [ "$CONTRIBFROMSOURCE" = "yes" ]; then
    make fetch
    make -j$JOBS .gettext
    make -j$JOBS

    if [ "$PACKAGE" = "yes" ]; then
        make package
    fi

else
if [ ! -e "../$TRIPLET" ]; then
    make prebuilt > $out
fi
fi
spopd

unset CFLAGS
unset CXXFLAGS
unset OBJCFLAGS

# Enable debug symbols by default
export CFLAGS="-g"
export CXXFLAGS="-g"
export OBJCFLAGS="-g"

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

    find VLC.app/ -name "*.dylib" -exec strip -x {} \;
    find VLC.app/ -type f -name "VLC" -exec strip -x {} \;
    find VLC.app/ -type f -name "Sparkle" -exec strip -x {} \;
    find VLC.app/ -type f -name "Growl" -exec strip -x {} \;
    find VLC.app/ -type f -name "Breakpad" -exec strip -x {} \;

    bin/vlc-cache-gen VLC.app/Contents/MacOS/plugins

    info "Building VLC release archive"
    make package-macosx-release
    shasum -a 512 vlc-*-release.zip
elif [ "$PACKAGETYPE" = "n" -o "$PACKAGE" = "yes" ]; then
    info "Building VLC dmg package"
    make package-macosx
fi

if [ ! -z "$VLCBUILDDIR" ];then
    popd
fi
