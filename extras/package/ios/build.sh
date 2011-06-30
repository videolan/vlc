#!/bin/sh
set -e

PLATFORM=OS
SDK=iphoneos3.2
VERBOSE=no

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
             SDK=iphonesimulator3.2
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

info "Building libvlc for the iOS"

if [ "$PLATFORM" = "Simulator" ]; then
    TARGET="i686-apple-darwin10"
    ARCH="i386"
else
    TARGET="arm-apple-darwin10"
    ARCH="armv7"
    OPTIM="-mno-thumb"
fi

# Test if SDK exists
xcodebuild -find gcc -sdk ${SDK} > ${out}

SDK_VERSION=`echo ${SDK} | sed -e 's/iphoneos//' -e 's/iphonesimulator//'`

info "Using ${ARCH} with SDK version ${SDK_VERSION}"

THIS_SCRIPT_PATH=`pwd`/$0

spushd `dirname ${THIS_SCRIPT_PATH}`/../../..
VLCROOT=`pwd` # Let's make sure VLCROOT is an absolute path
spopd

DEVROOT="/Developer/Platforms/iPhone${PLATFORM}.platform/Developer"
IOS_SDK_ROOT="${DEVROOT}/SDKs/iPhone${PLATFORM}${SDK_VERSION}.sdk"

BUILDDIR="${VLCROOT}/build-ios-${PLATFORM}"

PREFIX="${VLCROOT}/install-ios-${PLATFORM}"

IOS_GAS_PREPROCESSOR="${VLCROOT}/extras/package/ios/resources/gas-preprocessor.pl"

export AR="${DEVROOT}/usr/bin/ar"
export RANLIB="${DEVROOT}/usr/bin/ranlib"
export CFLAGS="-isysroot ${IOS_SDK_ROOT} -arch ${ARCH} -miphoneos-version-min=3.2 ${OPTIM}"
export OBJCFLAGS="${CFLAGS}"
if [ "$PLATFORM" = "Simulator" ]; then
    # Use the new ABI on simulator, else we can't build
    export OBJCFLAGS="-fobjc-abi-version=2 -fobjc-legacy-dispatch ${OBJCFLAGS}"
fi
export CPPFLAGS="${CFLAGS}"
export CXXFLAGS="${CFLAGS}"
export CPP="${DEVROOT}/usr/bin/cpp-4.2"
export CXXCPP="${DEVROOT}/usr/bin/cpp-4.2"

export CC="${DEVROOT}/usr/bin/gcc-4.2"
export OBJC="${DEVROOT}/usr/bin/gcc-4.2"
export CXX="${DEVROOT}/usr/bin/g++-4.2"
export LD="${DEVROOT}/usr/bin/ld"
export STRIP="${DEVROOT}/usr/bin/strip"

if [ "$PLATFORM" = "OS" ]; then
  export LDFLAGS="-L${IOS_SDK_ROOT}/usr/lib -arch ${ARCH}"
else
  export LDFLAGS="-syslibroot=${IOS_SDK_ROOT}/ -arch ${ARCH}"
fi

export PATH="/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/X11/bin:${VLCROOT}/extras/contrib/build/bin:${VLCROOT}/extras/package/ios/resources"

spushd ${VLCROOT}/extras/contrib

# contains gas-processor.pl
export PATH=$PATH:${VLCROOT}/extras/package/ios/resources

# The contrib will read the following
export IOS_SDK_ROOT

info "Building contrib for iOS in '${VLCROOT}/contrib-builddir-ios-${TARGET}'"

./bootstrap -t ${TARGET} -d ios \
   -b "${VLCROOT}/contrib-builddir-ios-${TARGET}" \
   -i "${VLCROOT}/contrib-ios-${TARGET}" > ${out}
spushd "${VLCROOT}/contrib-builddir-ios-${TARGET}"
make src > ${out}
spopd

info "Building contrib for current host"
./bootstrap > ${out}
make > ${out}

spopd

if [ "$PLATFORM" = "OS" ]; then
  export AS="${IOS_GAS_PREPROCESSOR} ${CC}"
  export ASCPP="${IOS_GAS_PREPROCESSOR} ${CC}"
else
  export AS="${DEVROOT}/usr/bin/as"
  export ASCPP="${DEVROOT}/usr/bin/as"
fi


info "Bootstraping vlc"

if ! [ -e ${VLCROOT}/configure ]; then
    ${VLCROOT}/bootstrap > ${out}
fi

if [ ".$PLATFORM" != ".Simulator" ]; then
    # FIXME - Do we still need this?
    export AVCODEC_CFLAGS="-I${PREFIX}/include"
    export AVCODEC_LIBS="-L${PREFIX}/lib -lavcodec -lavutil -lz"
    export AVFORMAT_CFLAGS="-I${PREFIX}/include"
    export AVFORMAT_LIBS="-L${PREFIX}/lib -lavcodec -lz -lavutil -lavformat"
fi

mkdir -p ${BUILDDIR}
spushd ${BUILDDIR}

# Run configure only upon changes.
if [ "${VLCROOT}/configure" -nt config.log -o \
     "${THIS_SCRIPT_PATH}" -nt config.log ]; then
CONTRIB_DIR=${VLCROOT}/contrib-ios-${TARGET} \
${VLCROOT}/configure \
    --prefix="${PREFIX}" \
    --host="${TARGET}" \
    --enable-debug \
    --enable-static-modules \
    --disable-macosx \
    --disable-macosx-defaults \
    --disable-macosx-vout \
    --disable-macosx-dialog-provider \
    --disable-macosx-qtcapture \
    --disable-macosx-eyetv \
    --disable-macosx-vlc-app \
    --with-macosx-sdk=${IOS_SDK_ROOT} \
    --enable-audioqueue \
    --enable-ios-vout \
    --enable-avcodec \
    --enable-avformat \
    --enable-swscale \
    --enable-faad \
    --disable-mad \
    --disable-a52 \
    --disable-fribidi \
    --disable-macosx-audio \
    --disable-qt4 --disable-skins2 \
    --disable-libgcrypt \
    --disable-remoteosd \
    --disable-vcd \
    --disable-postproc \
    --disable-vlc \
    --disable-vlm \
    --disable-httpd \
    --disable-nls \
    --disable-glx \
    --disable-visual \
    --disable-lua \
    --disable-sse \
    --disable-neon \
    --disable-mmx > ${out} # MMX and SSE support requires llvm which is broken on Simulator
fi

CORE_COUNT=`sysctl -n machdep.cpu.core_count`
let MAKE_JOBS=$CORE_COUNT+1

info "Building libvlc"
make -j$MAKE_JOBS > ${out}

info "Installing libvlc"
make install > ${out}
popd
