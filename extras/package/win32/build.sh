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
   -a <arch>     Use the specified arch (default: x86_64, possible i686)
   -p            Use a Prebuilt contrib package (speeds up compilation)
   -c            Create a Prebuilt contrib package (rarely used)
   -l            Enable translations (can be slow)
   -i <n|r>      Create an Installer (n: nightly, r: release)
EOF
}

ARCH="x86_64"
while getopts "hra:pcli:" OPTION
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
    *)
        usage
        exit 1
esac

#####

JOBS=`nproc --all`
TRIPLET=$ARCH-w64-mingw32

info "Building extra tools"
cd extras/tools
./bootstrap
make -j$JOBS
export PATH=$PWD/build/bin:$PATH
cd ../../

info "Building contribs"
export USE_FFMPEG=1
mkdir -p contrib/contrib-$SHORTARCH && cd contrib/contrib-$SHORTARCH
../bootstrap --host=$TRIPLET

# Rebuild the contribs or use the prebuilt ones
if [ "$PREBUILT" != "yes" ]; then
make list
make -j$JOBS fetch
make -j$JOBS
if [ "$PACKAGE" = "yes" ]; then
make package
fi
else
make prebuilt
make .luac
fi
cd ../..

info "Bootstrapping"
export PKG_CONFIG_LIBDIR=$PWD/contrib/$TRIPLET/lib/pkgconfig
export PATH=$PWD/contrib/$TRIPLET/bin:$PATH
echo $PATH

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
../extras/package/win32/configure.sh --host=$TRIPLET $CONFIGFLAGS

info "Compiling"
make -j$JOBS

if [ "$INSTALLER" = "n" ]; then
make package-win32-debug package-win32
elif [ "$INSTALLER" = "r" ]; then
make package-win32
fi
