#!/bin/sh
set -e

PLATFORM=OS
VERBOSE=no
SDK_VERSION=6.1
SDK_MIN=5.1
ARCH=armv7

usage()
{
cat << EOF
usage: $0 [-s] [-k sdk]

OPTIONS
   -k <sdk>      Specify which sdk to use ('xcodebuild -showsdks', current: ${SDK})
   -s            Build for simulator
   -a <arch>     Specify which arch to use (current: ${ARCH})
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

while getopts "hvsk:a:" OPTION
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
         a)
             ARCH=$OPTARG
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
    OPTIM="-O3 -g"
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

BUILDDIR="${VLCROOT}/build-ios-${PLATFORM}/${ARCH}"

PREFIX="${VLCROOT}/install-ios-${PLATFORM}/${ARCH}"

export PATH="${VLCROOT}/extras/tools/build/bin:${VLCROOT}/contrib/${TARGET}/bin:/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin"

# contains gas-processor.pl
export PATH=$PATH:${VLCROOT}/extras/package/ios/resources

info "Building tools"
spushd "${VLCROOT}/extras/tools"
./bootstrap
make && make .gas
spopd

info "Building contrib for iOS in '${VLCROOT}/contrib/iPhone${PLATFORM}-${ARCH}'"

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

if [ "$PLATFORM" = "OS" ]; then
    EXTRA_CFLAGS="-arch ${ARCH} -mcpu=cortex-a8"
    EXTRA_LDFLAGS="-arch ${ARCH}"
else
    EXTRA_CFLAGS="-m32"
    EXTRA_LDFLAGS="-m32"
fi

info "LD FLAGS SELECTED = '${LDFLAGS}'"

spushd ${VLCROOT}/contrib

echo ${VLCROOT}
mkdir -p "${VLCROOT}/contrib/iPhone${PLATFORM}-${ARCH}"
cd "${VLCROOT}/contrib/iPhone${PLATFORM}-${ARCH}"

if [ "$PLATFORM" = "OS" ]; then
      export AS="gas-preprocessor.pl ${CC}"
      export ASCPP="gas-preprocessor.pl ${CC}"
      export CCAS="gas-preprocessor.pl ${CC}"
else
  export AS="xcrun as"
  export ASCPP="xcrun as"
fi

../bootstrap --host=${TARGET} --build="i686-apple-darwin10" --prefix=${VLCROOT}/contrib/${TARGET}-${ARCH} --disable-gpl \
    --disable-disc --disable-sout \
    --enable-small \
    --disable-sdl \
    --disable-SDL_image \
    --disable-iconv \
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
    --disable-chromaprint \
    --disable-mad \
    --enable-fribidi \
    --enable-libxml2 \
    --enable-freetype2 \
    --enable-ass \
    --disable-fontconfig \
    --disable-taglib > ${out}

echo "EXTRA_CFLAGS += ${EXTRA_CFLAGS}" >> config.mak
echo "EXTRA_LDFLAGS += ${EXTRA_LDFLAGS}" >> config.mak
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

mkdir -p ${BUILDDIR}
spushd ${BUILDDIR}

info ">> --prefix=${PREFIX} --host=${TARGET}"

# Run configure only upon changes.
if [ "${VLCROOT}/configure" -nt config.log -o \
     "${THIS_SCRIPT_PATH}" -nt config.log ]; then
${VLCROOT}/configure \
    --prefix="${PREFIX}" \
    --host="${TARGET}" \
    --with-contrib="${VLCROOT}/contrib/${TARGET}-${ARCH}" \
    --disable-debug \
    --enable-static \
    --disable-macosx \
    --disable-macosx-vout \
    --disable-macosx-dialog-provider \
    --disable-macosx-qtkit \
    --disable-macosx-eyetv \
    --disable-macosx-vlc-app \
    --disable-macosx-avfoundation \
    --enable-audioqueue \
    --enable-ios-audio \
    --enable-ios-vout \
    --enable-ios-vout2 \
    --disable-shared \
    --disable-macosx-quartztext \
    --enable-avcodec \
    --enable-mkv \
    --enable-opus \
    --disable-sout \
    --disable-faad \
    --disable-lua \
    --disable-a52 \
    --enable-fribidi \
    --disable-macosx-audio \
    --disable-qt --disable-skins2 \
    --disable-libgcrypt \
    --disable-vcd \
    --disable-vlc \
    --disable-vlm \
    --disable-httpd \
    --disable-nls \
    --disable-glx \
    --disable-sse \
    --enable-neon \
    --disable-notify \
    --enable-live555 \
    --enable-realrtsp \
    --enable-dvbpsi \
    --enable-swscale \
    --disable-projectm \
    --enable-libass \
    --enable-libxml2 \
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
    --disable-mtp \
    --enable-ogg \
    --enable-speex \
    --enable-theora \
    --enable-flac \
    --disable-screen \
    --enable-freetype \
    --disable-taglib \
    --disable-mmx \
    --disable-mad > ${out} # MMX and SSE support requires llvm which is broken on Simulator
fi

CORE_COUNT=`sysctl -n machdep.cpu.core_count`
let MAKE_JOBS=$CORE_COUNT+1

info "Building libvlc"
make -j$MAKE_JOBS > ${out}

info "Installing libvlc"
make install > ${out}

find ${PREFIX}/lib/vlc/plugins -name *.a -type f -exec cp '{}' ${PREFIX}/lib/vlc/plugins \;
cp -R "${VLCROOT}/contrib/${TARGET}-${ARCH}" "${PREFIX}/contribs"

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
remap
"

for i in ${blacklist}
do
    find ${PREFIX}/lib/vlc/plugins -name *$i* -type f -exec rm '{}' \;
done

popd
