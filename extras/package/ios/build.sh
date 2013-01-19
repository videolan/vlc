#!/bin/sh
set -e

PLATFORM=OS
VERBOSE=no
SDK_VERSION=6.0
SDK_MIN=5.1

usage()
{
cat << EOF
usage: $0 [-s] [-k sdk]

OPTIONS
   -k       Specify which sdk to use ('xcodebuild -showsdks', current: ${SDK})
   -s       Build for simulator
EOF
}

spushd()
{
    pushd "$1" 2>&1> /dev/null
}

spopd()
{
    popd 2>&1> /dev/null
}

info()
{
    local blue="\033[1;34m"
    local normal="\033[0m"
    echo "[${blue}info${normal}] $1"
}

while getopts "hvsk:" OPTION
do
     case $OPTION in
         h)
             usage
             exit 1
             ;;
         v)
             VERBOSE=yes
             ;;
         s)
             PLATFORM=Simulator
             SDK=${SDK_MIN}
             ;;
         k)
             SDK=$OPTARG
             ;;
         ?)
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

out="/dev/null"
if [ "$VERBOSE" = "yes" ]; then
   out="/dev/stdout"
fi

info "Building libvlc for iOS"

if [ "$PLATFORM" = "Simulator" ]; then
    TARGET="i686-apple-darwin11"
    ARCH="i386"
    OPTIM="-O3 -g"
else
    TARGET="arm-apple-darwin11"
    ARCH="armv7 -g"
fi

info "Using ${ARCH} with SDK version ${SDK_VERSION}"

THIS_SCRIPT_PATH=`pwd`/$0

spushd `dirname ${THIS_SCRIPT_PATH}`/../../..
VLCROOT=`pwd` # Let's make sure VLCROOT is an absolute path
spopd

if test -z "$SDKROOT"
then
    SDKROOT=`xcode-select -print-path`/Platforms/iPhone${PLATFORM}.platform/Developer/SDKs/iPhone${PLATFORM}${SDK_VERSION}.sdk
    echo "SDKROOT not specified, assuming $SDKROOT"
fi

if [ ! -d "${SDKROOT}" ]
then
    echo "*** ${SDKROOT} does not exist, please install required SDK, or set SDKROOT manually. ***"
    exit 1
fi

BUILDDIR="${VLCROOT}/build-ios-${PLATFORM}"

PREFIX="${VLCROOT}/install-ios-${PLATFORM}"

IOS_GAS_PREPROCESSOR="${VLCROOT}/extras/tools/gas/gas-preprocessor.pl"

export PATH="${VLCROOT}/extras/tools/build/bin:${VLCROOT}/contrib/${TARGET}/bin:/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin"

# contains gas-processor.pl
export PATH=$PATH:${VLCROOT}/extras/package/ios/resources

info "Building tools"
spushd "${VLCROOT}/extras/tools"
./bootstrap
make && make .gas
spopd

info "Building contrib for iOS in '${VLCROOT}/contrib/iPhone${PLATFORM}'"

# The contrib will read the following
export AR="xcrun ar"

export RANLIB="xcrun ranlib"
export CC="xcrun clang"
export OBJC="xcrun clang"
export CXX="xcrun clang++"
export LD="xcrun ld"
export STRIP="xcrun strip"


export SDKROOT
if [ "$PLATFORM" = "OS" ]; then
export CFLAGS="-isysroot ${SDKROOT} -arch ${ARCH} -mcpu=cortex-a8 -miphoneos-version-min=${SDK_MIN} ${OPTIM}"
else
export CFLAGS="-isysroot ${SDKROOT} -arch ${ARCH} -miphoneos-version-min=${SDK_MIN} ${OPTIM}"
fi
export CPPFLAGS="${CFLAGS}"
export CXXFLAGS="${CFLAGS}"
export OBJCFLAGS="${CFLAGS}"

export CPP="xcrun cc -E"
export CXXCPP="xcrun c++ -E"

export BUILDFORIOS="yes"

if [ "$PLATFORM" = "Simulator" ]; then
    # Use the new ABI on simulator, else we can't build
    export OBJCFLAGS="-fobjc-abi-version=2 -fobjc-legacy-dispatch ${OBJCFLAGS}"
fi

if [ "$PLATFORM" = "OS" ]; then
  export LDFLAGS="-L${SDKROOT}/usr/lib -arch ${ARCH} -isysroot ${SDKROOT} -miphoneos-version-min=${SDK_MIN}"
else
  export LDFLAGS="-syslibroot=${SDKROOT}/ -arch ${ARCH} -miphoneos-version-min=${SDK_MIN}"
fi

info "LD FLAGS SELECTED = '${LDFLAGS}'"

spushd ${VLCROOT}/contrib

echo ${VLCROOT}
mkdir -p "${VLCROOT}/contrib/iPhone${PLATFORM}"
cd "${VLCROOT}/contrib/iPhone${PLATFORM}"

if [ "$PLATFORM" = "OS" ]; then
      export AS="${IOS_GAS_PREPROCESSOR} ${CC}"
      export ASCPP="${IOS_GAS_PREPROCESSOR} ${CC}"
      export CCAS="${IOS_GAS_PREPROCESSOR} ${CC}"
else
  export AS="xcrun as"
  export ASCPP="xcrun as"
fi

../bootstrap --host=${TARGET} --build="i686-apple-darwin10" --disable-disc --disable-sout \
    --enable-small \
    --disable-sdl \
    --disable-SDL_image \
    --disable-fontconfig \
    --disable-ass \
    --disable-freetype2 \
    --disable-iconv \
    --disable-fribidi \
    --disable-zvbi \
    --disable-kate \
    --disable-caca \
    --disable-gettext \
    --disable-mpcdec \
    --disable-upnp \
    --disable-gme \
    --disable-tremor \
    --disable-vorbis \
    --disable-sidplay2 \
    --disable-samplerate \
    --disable-goom \
    --disable-gcrypt \
    --disable-gnutls \
    --disable-orc \
    --disable-schroedinger \
    --disable-libmpeg2 \
    --enable-mad > ${out}
make
spopd

info "Bootstraping vlc"
pwd
info "VLCROOT = ${VLCROOT}"
if ! [ -e ${VLCROOT}/configure ]; then
    ${VLCROOT}/bootstrap  > ${out}
fi

info "Bootstraping vlc finished"

if [ ".$PLATFORM" != ".Simulator" ]; then
    # FIXME - Do we still need this?
    export AVCODEC_CFLAGS="-I${PREFIX}/include "
    export AVCODEC_LIBS="-L${PREFIX}/lib -lavcodec -lavutil -lz"
    export AVFORMAT_CFLAGS="-I${PREFIX}/include"
    export AVFORMAT_LIBS="-L${PREFIX}/lib -lavcodec -lz -lavutil -lavformat"
fi

export DVBPSI_CFLAGS="-I${VLCROOT}/contrib-ios-${TARGET}/include "
export DVBPSI_LIBS="-L${VLCROOT}/contrib-ios-${TARGET}/lib "

export SWSCALE_CFLAGS="-I${VLCROOT}/contrib-ios-${TARGET}/include "
export SWSCALE_LIBS="-L${VLCROOT}/contrib-ios-${TARGET}/lib "

mkdir -p ${BUILDDIR}
spushd ${BUILDDIR}

info ">> --prefix=${PREFIX} --host=${TARGET}"

# Run configure only upon changes.
if [ "${VLCROOT}/configure" -nt config.log -o \
     "${THIS_SCRIPT_PATH}" -nt config.log ]; then
CONTRIB_DIR=${VLCROOT}/contrib-ios-${TARGET} \
${VLCROOT}/configure \
    --prefix="${PREFIX}" \
    --host="${TARGET}" \
    --disable-debug \
    --enable-static \
    --disable-macosx \
    --disable-macosx-vout \
    --disable-macosx-dialog-provider \
    --disable-macosx-qtkit \
    --disable-macosx-eyetv \
    --disable-macosx-vlc-app \
    --enable-audioqueue \
    --enable-ios-vout \
    --disable-shared \
    --disable-macosx-quartztext \
    --enable-avcodec \
    --enable-mkv \
    --enable-opus \
    --enable-dvbpsi \
    --enable-swscale \
    --disable-projectm \
    --disable-sout \
    --disable-faad \
    --disable-lua \
    --enable-mad \
    --disable-a52 \
    --disable-fribidi \
    --disable-macosx-audio \
    --disable-qt --disable-skins2 \
    --disable-libgcrypt \
    --disable-vcd \
    --disable-vlc \
    --disable-vlm \
    --disable-httpd \
    --disable-nls \
    --disable-glx \
    --disable-lua \
    --disable-sse \
    --enable-neon \
    --disable-notify \
    --enable-live555 \
    --enable-realrtsp \
    --enable-dvbpsi \
    --enable-swscale \
    --disable-projectm \
    --disable-libass \
    --disable-sqlite \
    --disable-libxml2 \
    --disable-goom \
    --disable-dvdread \
    --disable-dvdnav \
    --disable-bluray \
    --disable-linsys \
    --disable-libva \
    --disable-gme \
    --disable-tremor \
    --disable-vorbis \
    --disable-fluidsynth \
    --disable-jack \
    --disable-pulse \
    --disable-sout \
    --disable-faad \
    --disable-lua \
    --disable-mtp \
    --enable-ogg \
    --enable-speex \
    --enable-theora \
    --enable-flac \
    --disable-freetype \
    --disable-taglib \
    --disable-mmx > ${out} # MMX and SSE support requires llvm which is broken on Simulator
fi

CORE_COUNT=`sysctl -n machdep.cpu.core_count`
let MAKE_JOBS=$CORE_COUNT+1

info "Building libvlc"
make -j$MAKE_JOBS > ${out}

info "Installing libvlc"
make install > ${out}
find ${PREFIX}/lib/vlc/plugins -name *.a -type f -exec cp '{}' ${PREFIX}/lib/vlc/plugins \;

info "Removing unneeded modules"
blacklist="
stats
access_bd
shm
access_imem
oldrc
real
hotkeys
gestures
sap
dynamicoverlay
rss
ball
marq
magnify
audiobargraph_
clone
mosaic
osdmenu
puzzle
mediadirs
t140
ripple
motion
sharpen
grain
posterize
mirror
wall
scene
blendbench
psychedelic
alphamask
netsync
audioscrobbler
motiondetect
motionblur
export
smf
podcast
bluescreen
erase
stream_filter_record
speex_resampler
remoteosd
magnify
gradient
tospdif
dtstofloat32
logger
visual
fb
aout_file
yuv
dummy
invert
sepia
wave
hqdn3d
headphone_channel_mixer
gaussianblur
gradfun
extract
colorthres
antiflicker
anaglyph
adjust
remap
"

for i in ${blacklist}
do
    find ${PREFIX}/lib/vlc/plugins -name *$i* -type f -exec rm '{}' \;
done

popd
