#!/bin/sh

set -e
set -x

info()
{
    local green="\033[1;32m"
    local normal="\033[0m"
    echo "[${green}build${normal}] $1"
}

usage()
{
cat << EOF
usage: $0 [options]

Build vlc in the current directory

OPTIONS:
   -h            Show some help
   -r            Release mode (default is debug)
   -a <arch>     Use the specified arch (default: x86_64, possible i686, aarch64)
   -p            Use a Prebuilt contrib package (speeds up compilation)
   -c            Create a Prebuilt contrib package (rarely used)
   -l            Enable translations (can be slow)
   -i <n|r|u>    Create an Installer (n: nightly, r: release, u: unsigned release archive)
   -s            Interactive shell (get correct environment variables for build)
   -b <url>      Enable breakpad support and send crash reports to this URL
EOF
}

ARCH="x86_64"
while getopts "hra:pcli:sb:" OPTION
do
     case $OPTION in
         h)
             usage
             exit 1
         ;;
         r)
             RELEASE="yes"
             INSTALLER="r"
         ;;
         a)
             ARCH=$OPTARG
         ;;
         p)
             PREBUILT="yes"
         ;;
         c)
             PACKAGE="yes"
         ;;
         l)
             I18N="yes"
         ;;
         i)
             INSTALLER=$OPTARG
         ;;
         s)
             INTERACTIVE="yes"
         ;;
         b)
             BREAKPAD=$OPTARG
         ;;
     esac
done
shift $(($OPTIND - 1))

if [ "x$1" != "x" ]; then
    usage
    exit 1
fi

case $ARCH in
    x86_64)
        SHORTARCH="win64"
        ;;
    i686)
        SHORTARCH="win32"
        ;;
    aarch64)
        SHORTARCH="winarm64"
        ;;
    *)
        usage
        exit 1
esac

#####

JOBS=`getconf _NPROCESSORS_ONLN 2>&1`
TRIPLET=$ARCH-w64-mingw32

info "Building extra tools"
cd extras/tools
# bootstrap only if needed in interactive mode
if [ "$INTERACTIVE" != "yes" ] || [ ! -f ./Makefile ]; then
    ./bootstrap
fi
make -j$JOBS
export PATH="$PWD/build/bin":"$PATH"
cd ../../

export USE_FFMPEG=1
export PKG_CONFIG_LIBDIR=$PWD/contrib/$TRIPLET/lib/pkgconfig
export PATH="$PWD/contrib/$TRIPLET/bin":"$PATH"

if [ "$INTERACTIVE" = "yes" ]; then
if [ "x$SHELL" != "x" ]; then
    exec $SHELL
else
    exec /bin/sh
fi
fi

info "Building contribs"
echo $PATH

mkdir -p contrib/contrib-$SHORTARCH && cd contrib/contrib-$SHORTARCH
if [ ! -z "$BREAKPAD" ]; then
     CONTRIBFLAGS="$CONTRIBFLAGS --enable-breakpad"
fi
../bootstrap --host=$TRIPLET $CONTRIBFLAGS

# Rebuild the contribs or use the prebuilt ones
if [ "$PREBUILT" != "yes" ]; then
make list
make -j$JOBS fetch
make -j$JOBS -k || make -j1
if [ "$PACKAGE" = "yes" ]; then
make package
fi
elif [ -n "$VLC_PREBUILT_CONTRIBS_URL" ]; then
make prebuilt PREBUILT_URL="$VLC_PREBUILT_CONTRIBS_URL"
make .luac
else
make prebuilt
make .luac
fi
cd ../..

info "Bootstrapping"

./bootstrap

info "Configuring VLC"
mkdir $SHORTARCH || true
cd $SHORTARCH

CONFIGFLAGS=""
if [ "$RELEASE" != "yes" ]; then
     CONFIGFLAGS="$CONFIGFLAGS --enable-debug"
fi
if [ "$I18N" != "yes" ]; then
     CONFIGFLAGS="$CONFIGFLAGS --disable-nls"
fi
if [ ! -z "$BREAKPAD" ]; then
     CONFIGFLAGS="$CONFIGFLAGS --with-breakpad=$BREAKPAD"
fi

../extras/package/win32/configure.sh --host=$TRIPLET $CONFIGFLAGS

info "Compiling"
make -j$JOBS

if [ "$INSTALLER" = "n" ]; then
make package-win32-debug package-win32 package-msi
elif [ "$INSTALLER" = "r" ]; then
make package-win32
elif [ "$INSTALLER" = "u" ]; then
make package-win32-release
sha512sum vlc-*-release.7z
fi
