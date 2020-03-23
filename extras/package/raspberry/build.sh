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
   -a <arch>     Use the specified arch (default: arm, possible aarch64)
   -p            Use a Prebuilt contrib package (speeds up compilation)
   -c            Create a Prebuilt contrib package (rarely used)
   -l            Enable translations (can be slow)
   -s            Interactive shell (get correct environment variables for build)
   -x            Add extra checks when compiling
EOF
}

ARCH="arm"
while getopts "hra:pcli:sdx" OPTION
do
     case $OPTION in
         h)
             usage
             exit 1
         ;;
         r)
             RELEASE="yes"
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
         s)
             INTERACTIVE="yes"
         ;;
         x)
             EXTRA_CHECKS="yes"
         ;;
     esac
done
shift $(($OPTIND - 1))

if [ "x$1" != "x" ]; then
    usage
    exit 1
fi

case $ARCH in
    aarch64)
        SHORTARCH="linuxarm64"
        EABI="gnu"
        ;;
    arm)
        SHORTARCH="linuxarm"
        EABI="gnueabihf"
        ;;
    *)
        usage
        exit 1
esac

#####

SCRIPT_PATH="$( cd "$(dirname "$0")" ; pwd -P )"

: ${JOBS:=$(getconf _NPROCESSORS_ONLN 2>&1)}
TRIPLET=$ARCH-linux-$EABI

info "Building extra tools"
mkdir -p extras/tools
cd extras/tools
export PATH="$PWD/build/bin":"$PATH"
if [ "$INTERACTIVE" != "yes" ] || [ ! -f ./Makefile ]; then
    ${SCRIPT_PATH}/../../tools/bootstrap
fi
make -j$JOBS --output-sync=recurse
cd ../..

export USE_FFMPEG=1

if [ "$INTERACTIVE" = "yes" ]; then
if [ "x$SHELL" != "x" ]; then
    exec $SHELL
else
    exec /bin/sh
fi
fi

info "Building contribs"

mkdir -p contrib/contrib-$SHORTARCH && cd contrib/contrib-$SHORTARCH

# issue with arm detection of the target (detects i686)
CONTRIBFLAGS="$CONTRIBFLAGS --disable-x265"

${SCRIPT_PATH}/../../../contrib/bootstrap --host=$TRIPLET $CONTRIBFLAGS

# use the system headers for the OS and firmware
export CFLAGS="$CFLAGS -g -mfpu=neon -isystem=/usr/lib/$TRIPLET -isystem=/opt/vc/include"
export CXXFLAGS="$CXXFLAGS -g -mfpu=neon -isystem=/usr/lib/$TRIPLET -isystem=/opt/vc/include"
export CPPFLAGS="$CPPFLAGS -g -mfpu=neon -isystem=/usr/lib/$TRIPLET -isystem=/opt/vc/include"
export LDFLAGS="$LDFLAGS -L/usr/$TRIPLET/lib -L/opt/vc/lib"

# Rebuild the contribs or use the prebuilt ones
if [ "$PREBUILT" != "yes" ]; then
    make list
    make -j$JOBS --output-sync=recurse fetch
    make -j$JOBS --output-sync=recurse -k || make -j1
    if [ "$PACKAGE" = "yes" ]; then
        make package
    fi
elif [ -n "$VLC_PREBUILT_CONTRIBS_URL" ]; then
    make prebuilt PREBUILT_URL="$VLC_PREBUILT_CONTRIBS_URL"
    make -j$JOBS --output-sync=recurse .luac
else
    make prebuilt
    make -j$JOBS --output-sync=recurse .luac
fi
cd ../..

info "Bootstrapping"
if ! [ -e ${SCRIPT_PATH}/../../../configure ]; then
    echo "Bootstraping vlc"
    ${SCRIPT_PATH}/../../../bootstrap
fi

info "Configuring VLC"
mkdir $SHORTARCH || true
cd $SHORTARCH

if [ "$RELEASE" != "yes" ]; then
     CONFIGFLAGS="$CONFIGFLAGS --enable-debug"
else
     CONFIGFLAGS="$CONFIGFLAGS --disable-debug"
fi
if [ "$I18N" != "yes" ]; then
     CONFIGFLAGS="$CONFIGFLAGS --disable-nls"
fi
if [ ! -z "$EXTRA_CHECKS" ]; then
    CFLAGS="$CFLAGS -Werror=incompatible-pointer-types -Werror=missing-field-initializers"
fi

ac_cv_path_MOC="qtchooser -qt=qt5-$TRIPLET -run-tool=moc" \
ac_cv_path_RCC="qtchooser -qt=qt5-$TRIPLET -run-tool=rcc" \
ac_cv_path_UIC="qtchooser -qt=qt5-$TRIPLET -run-tool=uic" \
${SCRIPT_PATH}/configure.sh --host=$TRIPLET $CONFIGFLAGS

info "Compiling"
make -j$JOBS
