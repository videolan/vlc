#!/bin/sh
set -e

PLATFORM=OS
VERBOSE=no
DEBUG=no
SDK_VERSION=`xcrun --sdk iphoneos --show-sdk-version`
SDK_MIN=7.0
SIXTYFOURBIT_SDK_MIN=7.0
ARCH=armv7
SCARY=yes

CORE_COUNT=`sysctl -n machdep.cpu.core_count`
let MAKE_JOBS=$CORE_COUNT+1

usage()
{
cat << EOF
usage: $0 [-s] [-d] [-v] [-k sdk]

OPTIONS
   -k <sdk version>      Specify which sdk to use ('xcodebuild -showsdks', current: ${SDK_VERSION})
   -s            Build for simulator
   -a <arch>     Specify which arch to use (current: ${ARCH})
   -d            Enable debug
   -v            Enable verbose command-line output
   -w            Build a limited stack of non-scary libraries only
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

while getopts "hvwdsk:a:" OPTION
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
             ;;
         d)
             DEBUG=yes
             ;;
         w)
             SCARY=no
             ;;
         k)
             SDK_VERSION=$OPTARG
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

TARGET="${ARCH}-apple-darwin11"

# apple doesn't call AArch64 that way, but arm64 (a contrario to all libraries)
# so we need to translate it..
if [ "$ARCH" = "aarch64" ]; then
	ACTUAL_ARCH="arm64"
else
	ACTUAL_ARCH="$ARCH"
fi

if [ "$DEBUG" = "yes" ]; then
    OPTIM="-O0 -g"
else
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

BUILDDIR="${VLCROOT}/build-ios-${PLATFORM}/${ACTUAL_ARCH}"

PREFIX="${VLCROOT}/install-ios-${PLATFORM}/${ACTUAL_ARCH}"

export PATH="${VLCROOT}/extras/tools/build/bin:${VLCROOT}/contrib/${TARGET}/bin:/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin"

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

export PLATFORM=$PLATFORM
export SDK_VERSION=$SDK_VERSION

export CFLAGS="-isysroot ${SDKROOT} -arch ${ACTUAL_ARCH} ${OPTIM}"

if [ "$PLATFORM" = "OS" ]; then
if [ "$ARCH" != "aarch64" ]; then
export CFLAGS="${CFLAGS} -mcpu=cortex-a8 -miphoneos-version-min=${SDK_MIN}"
else
export CFLAGS="${CFLAGS} -miphoneos-version-min=${SIXTYFOURBIT_SDK_MIN}"
fi
else
export CFLAGS="${CFLAGS} -miphoneos-version-min=${SIXTYFOURBIT_SDK_MIN}"
fi

export CXXFLAGS="${CFLAGS}"
export CPPFLAGS="${CFLAGS}"

export CPP="xcrun cc -E"
export CXXCPP="xcrun c++ -E"

export BUILDFORIOS="yes"

if [ "$PLATFORM" = "Simulator" ]; then
    # Use the new ABI on simulator, else we can't build
    export OBJCFLAGS="-fobjc-abi-version=2 -fobjc-legacy-dispatch ${OBJCFLAGS}"
fi

export LDFLAGS="-L${SDKROOT}/usr/lib -arch ${ACTUAL_ARCH} -isysroot ${SDKROOT}"

if [ "$PLATFORM" = "OS" ]; then
    EXTRA_CFLAGS="-arch ${ACTUAL_ARCH}"
    EXTRA_LDFLAGS="-arch ${ACTUAL_ARCH}"
if [ "$ARCH" != "aarch64" ]; then
    EXTRA_CFLAGS+=" -mcpu=cortex-a8"
    EXTRA_CFLAGS+=" -miphoneos-version-min=${SDK_MIN}"
    EXTRA_LDFLAGS+=" -Wl,-ios_version_min,${SDK_MIN}"
    export LDFLAGS="${LDFLAGS} -Wl,-ios_version_min,${SDK_MIN}"
else
    EXTRA_CFLAGS+=" -miphoneos-version-min=${SIXTYFOURBIT_SDK_MIN}"
    EXTRA_LDFLAGS+=" -Wl,-ios_version_min,${SIXTYFOURBIT_SDK_MIN}"
    export LDFLAGS="${LDFLAGS} -Wl,-ios_version_min,${SIXTYFOURBIT_SDK_MIN}"
fi
else
    EXTRA_CFLAGS="-arch ${ARCH}"
    EXTRA_CFLAGS+=" -miphoneos-version-min=${SIXTYFOURBIT_SDK_MIN}"
    EXTRA_LDFLAGS=" -Wl,-ios_version_min,${SIXTYFOURBIT_SDK_MIN}"
    export LDFLAGS="${LDFLAGS} -Wl-ios_version_min,${SIXTYFOURBIT_SDK_MIN}"
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
    if [ "$ARCH" = "aarch64" ]; then
        export GASPP_FIX_XCODE5=1
    fi
else
    export ASCPP="xcrun as"
fi

../bootstrap --build=x86_64-apple-darwin11 --host=${TARGET} --prefix=${VLCROOT}/contrib/${TARGET}-${ARCH} --disable-gpl \
    --disable-disc --disable-sout \
    --disable-sdl \
    --disable-SDL_image \
    --disable-iconv \
    --enable-zvbi \
    --disable-kate \
    --disable-caca \
    --disable-gettext \
    --disable-mpcdec \
    --disable-upnp \
    --disable-gme \
    --disable-tremor \
    --enable-vorbis \
    --disable-sidplay2 \
    --disable-samplerate \
    --disable-goom \
    --disable-vncserver \
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
    --disable-gpg-error \
    --disable-lua \
    --disable-protobuf \
    --disable-aribb24 \
    --disable-aribb25 \
    --enable-vpx \
    --enable-libdsm \
    --enable-taglib > ${out}

echo "EXTRA_CFLAGS += ${EXTRA_CFLAGS}" >> config.mak
echo "EXTRA_LDFLAGS += ${EXTRA_LDFLAGS}" >> config.mak
make fetch
make -j$MAKE_JOBS > ${out}
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

if [ "$DEBUG" = "yes" ]; then
    DEBUGFLAG="--enable-debug"
else
    DEBUGFLAG="--disable-debug"
fi

if [ "$SCARY" = "yes" ]; then
	SCARYFLAG="--enable-dvbpsi --enable-avcodec"
else
	SCARYFLAG="--disable-dca --disable-dvbpsi --disable-avcodec --disable-avformat --disable-zvbi --enable-vpx"
fi

# Run configure only upon changes.
if [ "${VLCROOT}/configure" -nt config.log -o \
     "${THIS_SCRIPT_PATH}" -nt config.log ]; then
${VLCROOT}/configure \
    --prefix="${PREFIX}" \
    --host="${TARGET}" \
    --with-contrib="${VLCROOT}/contrib/${TARGET}-${ARCH}" \
    --enable-static \
    ${DEBUGFLAG} \
    ${SCARYFLAG} \
    --disable-macosx \
    --disable-macosx-dialog-provider \
    --disable-macosx-qtkit \
    --disable-macosx-eyetv \
    --disable-macosx-vlc-app \
    --disable-macosx-avfoundation \
    --disable-audioqueue \
    --disable-shared \
    --enable-macosx-quartztext \
    --enable-mkv \
    --enable-opus \
    --disable-faad \
    --disable-lua \
    --disable-a52 \
    --enable-fribidi \
    --disable-qt --disable-skins2 \
    --disable-vcd \
    --disable-vlc \
    --disable-vlm \
    --disable-httpd \
    --disable-nls \
    --disable-sse \
    --enable-neon \
    --disable-notify \
    --enable-live555 \
    --enable-realrtsp \
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
    --enable-vorbis \
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
    --enable-taglib \
    --disable-mmx \
    --disable-addonmanagermodules \
    --disable-mad > ${out} # MMX and SSE support requires llvm which is broken on Simulator
fi

info "Building libvlc"
make -j$MAKE_JOBS > ${out}

info "Installing libvlc"
make install > ${out}

find ${PREFIX}/lib/vlc/plugins -name *.a -type f -exec cp '{}' ${PREFIX}/lib/vlc/plugins \;
rm -rf "${PREFIX}/contribs"
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
oldmovie
vhs
demuxdump
fingerprinter
output_udp
output_http
output_livehttp
libmux
stream_out
"

if [ "$SCARY" = "no" ]; then
blacklist="${blacklist}
dts
dvbsub
svcd
hevc
packetizer_mlp
a52
vc1
uleaddvaudio
librar
libvoc
avio
chorus_flanger
smooth
cvdsub
libmod
libdash
libmpgv
dolby_surround
mpeg_audio"
fi

echo ${blacklist}

for i in ${blacklist}
do
    find ${PREFIX}/lib/vlc/plugins -name *$i* -type f -exec rm '{}' \;
done

popd
