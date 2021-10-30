#!/usr/bin/env bash
# Copyright (C) Marvin Scholz
#
# Script to help build VLC or libVLC for Apple OSes
# Supported OSes: iOS, tvOS, macOS
#
# Currently this script builds a full static library,
# with all modules and contribs combined into one .a
# file.
#
# Settings that need to be changed from time to time,
# like the target OS versions or contrib/configure options
# can be found in the build.conf file in the same folder.

# TODO:
# - Add packaging support and right options to build a macOS app
# - Support shared build where you get a dylib for libvlc,
#   libvlccore and dylibs for the individual modules.
# - Support a mixed shared build where you only have a
#   libvlc.dylib that includes libvlccore and the modules
#   statically.
# Proposed interface for this:
#   --buildmode=<fullstatic, pseudoshared, shared>
#        fullstatic: One resulting static library with libvlc and modules
#        pseudoshared: Shared library with all modules statically linked
#        shared: Shared libraries and modules

# Dir of this script
readonly VLC_SCRIPT_DIR="$(cd "${BASH_SOURCE%/*}"; pwd)"

# Verify script run location
[ ! -f "$(pwd)/../src/libvlc.h" ] \
    && echo "ERROR: This script must be run from a" \
            "build subdirectory in the VLC source" >&2 \
    && exit 1

# Include vlc env script
. "$VLC_SCRIPT_DIR/../macosx/env.build.sh" "none"

# Include build config file
. "$VLC_SCRIPT_DIR/build.conf"

##########################################################
#                    Global variables                    #
##########################################################

# Name of this script
readonly VLC_SCRIPT_NAME=$(basename "$0")
# VLC source dir root
readonly VLC_SRC_DIR=$(vlcGetRootDir)
# VLC build dir
readonly VLC_BUILD_DIR=$(pwd)
# Whether verbose output is enabled or not
VLC_SCRIPT_VERBOSE=0
# Architecture of the host (OS that the result will run on)
VLC_HOST_ARCH="x86_64"
# Host platform information
VLC_HOST_PLATFORM=
VLC_HOST_TRIPLET=
VLC_BUILD_TRIPLET=
# Set to "1" when building for simulator
VLC_HOST_PLATFORM_SIMULATOR=
# The host OS name (without the simulator suffix)
# as used by the Apple tools for flags like the
# min version or clangs target option
VLC_HOST_OS=
# Lowest OS version (iOS, tvOS or macOS) to target
# Do NOT edit this to set a specific target, instead
# edit the VLC_DEPLOYMENT_TARGET_* variables above.
VLC_DEPLOYMENT_TARGET=
# Flags for linker and compiler that set the min target OS
# Those will be set by the set_deployment_target function
VLC_DEPLOYMENT_TARGET_LDFLAG=
VLC_DEPLOYMENT_TARGET_CFLAG=
# SDK name (optionally with version) to build with
# We default to macOS builds, so this is set to macosx
VLC_APPLE_SDK_NAME="macosx"
# SDK path
# Set in the validate_sdk_name function
VLC_APPLE_SDK_PATH=
# SDK version
# Set in the validate_sdk_name function
VLC_APPLE_SDK_VERSION=
# Indicated if prebuilt contribs package
# should be created
VLC_MAKE_PREBUILT_CONTRIBS=0
# Indicates that prebuit contribs should be
# used instead of building the contribs from source
VLC_USE_PREBUILT_CONTRIBS=0
# User-provided URL from where to fetch contribs, empty
# for the default chosen by contrib system
VLC_PREBUILT_CONTRIBS_URL=${VLC_PREBUILT_CONTRIBS_URL:-""}
# The number of cores to compile on
CORE_COUNT=$(sysctl -n machdep.cpu.core_count || nproc || echo 0)
let VLC_USE_NUMBER_OF_CORES=$CORE_COUNT+1
VLC_REQUESTED_CORE_COUNT=0
# whether to disable debug mode (the default) or not
VLC_DISABLE_DEBUG=0
VLC_MERGE_PLUGINS=0
# whether to compile with bitcode or not
VLC_USE_BITCODE=0
VLC_BITCODE_FLAG="-fembed-bitcode"
# whether to build static or dynamic plugins
VLC_BUILD_DYNAMIC=0
VLC_CONFIGURE_ONLY=0

# Tools to be used
VLC_HOST_CC="$(xcrun --find clang)"
VLC_HOST_CPP="$(xcrun --find clang) -E"
VLC_HOST_CXX="$(xcrun --find clang++)"
VLC_HOST_OBJC="$(xcrun --find clang)"
VLC_HOST_LD="$(xcrun --find ld)"
VLC_HOST_AR="$(xcrun --find ar)"
VLC_HOST_STRIP="$(xcrun --find strip)"
VLC_HOST_RANLIB="$(xcrun --find ranlib)"
VLC_HOST_NM="$(xcrun --find nm)"

##########################################################
#                    Helper functions                    #
##########################################################

# Print command line usage
usage()
{
    echo "Usage: $VLC_SCRIPT_NAME [options]"
    echo " --arch=ARCH      Architecture to build for"
    echo "                   (i386|x86_64|armv7|arm64)"
    echo " --sdk=SDK        Name of the SDK to build with (see 'xcodebuild -showsdks')"
    echo " --enable-bitcode        Enable bitcode for compilation, same as with =full"
    echo " --enable-bitcode=marker Enable bitcode marker for compilation"
    echo " --enable-merge-plugins Enable the merging of plugins into a single archive"
    echo " --disable-debug  Disable libvlc debug mode (for release)"
    echo " --configure      Only configure libvlc"
    echo " --verbose        Print verbose output and disable multi-core use"
    echo " --help           Print this help"
    echo ""
    echo "Advanced options:"
    echo " --package-contribs        Create a prebuilt contrib package"
    echo " --with-prebuilt-contribs  Use prebuilt contribs instead of building"
    echo "                           them from source"
    echo " --enable-shared           Build dynamic libraries and plugins"
    echo "Environment variables:"
    echo " VLC_PREBUILT_CONTRIBS_URL  URL to fetch the prebuilt contrib archive"
    echo "                            from when --with-prebuilt-contribs is used"
}

# Print error message and terminate script with status 1
# Arguments:
#   Message to print
abort_err()
{
    echo "ERROR: $1" >&2
    exit 1
}

# Print message if verbose, else silent
# Globals:
#   VLC_SCRIPT_VERBOSE
# Arguments:
#   Message to print
verbose_msg()
{
    if [ "$VLC_SCRIPT_VERBOSE" -gt "0" ]; then
        echo "$1"
    fi
}

# Check if tool exists, if not error out
# Arguments:
#   Tool name to check for
check_tool()
{
    command -v "$1" >/dev/null 2>&1 || {
        abort_err "This script requires '$1' but it was not found"
    }
}

# Set the VLC_DEPLOYMENT_TARGET* flag options correctly
# Globals:
#   VLC_DEPLOYMENT_TARGET
#   VLC_DEPLOYMENT_TARGET_LDFLAG
#   VLC_DEPLOYMENT_TARGET_CFLAG
# Arguments:
#   Deployment target version
set_deployment_target()
{
    VLC_DEPLOYMENT_TARGET="$1"
    VLC_DEPLOYMENT_TARGET_LDFLAG="-Wl,-$VLC_HOST_OS"
    VLC_DEPLOYMENT_TARGET_CFLAG="-m$VLC_HOST_OS"

    if [ -n "$VLC_HOST_PLATFORM_SIMULATOR" ]; then
        VLC_DEPLOYMENT_TARGET_LDFLAG="${VLC_DEPLOYMENT_TARGET_LDFLAG}_simulator"
        VLC_DEPLOYMENT_TARGET_CFLAG="${VLC_DEPLOYMENT_TARGET_CFLAG}-simulator"
    fi

    VLC_DEPLOYMENT_TARGET_LDFLAG="${VLC_DEPLOYMENT_TARGET_LDFLAG}_version_min,${VLC_DEPLOYMENT_TARGET}"
    VLC_DEPLOYMENT_TARGET_CFLAG="${VLC_DEPLOYMENT_TARGET_CFLAG}-version-min=${VLC_DEPLOYMENT_TARGET}"
}

# Validates the architecture and sets VLC_HOST_ARCH
# This MUST set the arch to what the compiler accepts
# for the -arch argument!
# Globals:
#   VLC_HOST_ARCH
# Arguments:
#   Architecture string
validate_architecture()
{
    case "$1" in
    i386|x86_64|armv7|arm64)
        VLC_HOST_ARCH="$1"
        ;;
    aarch64)
        VLC_HOST_ARCH="arm64"
        ;;
    *)
        abort_err "Invalid architecture '$1'"
        ;;
    esac
}

# Set the VLC_HOST_TRIPLET based on the architecture
# by querying the compiler for it, as the VLC_HOST_ARCH
# can not be used in the triplet directly, like in
# case of arm64.
# Globals:
#   CC
#   VLC_HOST_TRIPLET
# Arguments:
#   Architecture string
set_host_triplet()
{
    local triplet_arch=$(${VLC_HOST_CC:-cc} -arch "$1" -dumpmachine | cut -d- -f 1)
    # We can not directly use the compiler value here as when building for
    # x86_64 iOS Simulator the triplet will match the build machine triplet
    # exactly, which will cause autoconf to assume we are not cross-compiling.
    # Therefore we construct a triplet here without a version number, which
    # will not match the autoconf "guessed" host machine triplet.
    VLC_HOST_TRIPLET="${triplet_arch}-apple-darwin"
    VLC_HOST_TRIPLET="${VLC_HOST_TRIPLET/arm64/aarch64}"
}

# Set the VLC_BUILD_TRIPLET based on the architecture
# that we run on.
# Globals:
#   VLC_BUILD_TRIPLET
# Arguments:
#   None
set_build_triplet()
{
    local build_arch="$(uname -m | cut -d. -f1)"
    VLC_BUILD_TRIPLET="$(${VLC_HOST_CC} -arch "${build_arch}" -dumpmachine)"
    VLC_BUILD_TRIPLET="${VLC_BUILD_TRIPLET/arm64/aarch64}"
}

# Take SDK name, verify it exists and populate
# VLC_HOST_*, VLC_APPLE_SDK_PATH variables based
# on the SDK and calls the set_deployment_target
# function with the rigth target version
# Globals:
#   VLC_DEPLOYMENT_TARGET_IOS
#   VLC_DEPLOYMENT_TARGET_TVOS
#   VLC_DEPLOYMENT_TARGET_MACOSX
# Arguments:
#   SDK name
validate_sdk_name()
{
    xcrun --sdk "$1" --show-sdk-path >/dev/null 2>&1 || {
        abort_err "Failed to find SDK '$1'"
    }

    VLC_APPLE_SDK_PATH="$(xcrun --sdk "$1" --show-sdk-path)"
    VLC_APPLE_SDK_VERSION="$(xcrun --sdk "$1" --show-sdk-version)"
    if [ ! -d "$VLC_APPLE_SDK_PATH" ]; then
        abort_err "SDK at '$VLC_APPLE_SDK_PATH' does not exist"
    fi

    case "$1" in
        iphoneos*)
            VLC_HOST_PLATFORM="iOS"
            VLC_HOST_OS="ios"
            set_deployment_target "$VLC_DEPLOYMENT_TARGET_IOS"
            ;;
        iphonesimulator*)
            VLC_HOST_PLATFORM="iOS-Simulator"
            VLC_HOST_PLATFORM_SIMULATOR="yes"
            VLC_HOST_OS="ios"
            set_deployment_target "$VLC_DEPLOYMENT_TARGET_IOS"
            ;;
        appletvos*)
            VLC_HOST_PLATFORM="tvOS"
            VLC_HOST_OS="tvos"
            set_deployment_target "$VLC_DEPLOYMENT_TARGET_TVOS"
            ;;
        appletvsimulator*)
            VLC_HOST_PLATFORM="tvOS-Simulator"
            VLC_HOST_PLATFORM_SIMULATOR="yes"
            VLC_HOST_OS="tvos"
            set_deployment_target "$VLC_DEPLOYMENT_TARGET_TVOS"
            ;;
        macosx*)
            VLC_HOST_PLATFORM="macOS"
            VLC_HOST_OS="macosx"
            set_deployment_target "$VLC_DEPLOYMENT_TARGET_MACOSX"
            ;;
        watch*)
            abort_err "Building for watchOS is not supported by this script"
            ;;
        *)
            abort_err "Unhandled SDK name '$1'"
            ;;
    esac
}

# Set env variables used to define compilers and flags
# Arguments:
#   Additional flags for use with C-like compilers
# Globals:
#   VLC_DEPLOYMENT_TARGET_CFLAG
#   VLC_DEPLOYMENT_TARGET_LDFLAG
#   VLC_APPLE_SDK_PATH
#   VLC_HOST_ARCH
set_host_envvars()
{
    # Flags to be used for C-like compilers (C, C++, Obj-C)
    local clike_flags="$VLC_DEPLOYMENT_TARGET_CFLAG -arch $VLC_HOST_ARCH -isysroot $VLC_APPLE_SDK_PATH $1"
    local bitcode_flag=""
    if [ "$VLC_USE_BITCODE" -gt "0" ]; then
        clike_flags+=" $VLC_BITCODE_FLAG"
        bitcode_flag=" $VLC_BITCODE_FLAG"
    fi

    export CPPFLAGS="-arch $VLC_HOST_ARCH -isysroot $VLC_APPLE_SDK_PATH"

    export CFLAGS="$clike_flags -DI_CAN_HAZ_TSD"
    export CXXFLAGS="$clike_flags -DI_CAN_HAZ_TSD"
    export OBJCFLAGS="$clike_flags -DI_CAN_HAZ_TSD"

    # Vanilla clang doesn't use VLC_DEPLOYMENT_TAGET_LDFLAGS but only the CFLAGS variant
    export LDFLAGS="$VLC_DEPLOYMENT_TARGET_LDFLAG $VLC_DEPLOYMENT_TARGET_CFLAG -arch $VLC_HOST_ARCH ${bitcode_flag}"
}

hostenv()
{
    CC="${VLC_HOST_CC}" \
    CPP="${VLC_HOST_CPP}" \
    CXX="${VLC_HOST_CXX}" \
    OBJC="${VLC_HOST_OBJC}" \
    LD="${VLC_HOST_LD}" \
    AR="${VLC_HOST_AR}" \
    STRIP="${VLC_HOST_STRIP}" \
    RANLIB="${VLC_HOST_RANLIB}" \
    NM="${VLC_HOST_NM}" \
    "$@"
#VLC_BUILD_DIR}/build/ar.sh"

}

ac_var_to_export_ac_var()
{
    for ac_var in "$@"; do
        echo "export $ac_var"
    done
}

# Write config.mak for contribs
# Globals:
#   VLC_DEPLOYMENT_TARGET_CFLAG
#   VLC_DEPLOYMENT_TARGET_LDFLAG
#   VLC_APPLE_SDK_PATH
#   VLC_HOST_ARCH
write_config_mak()
{
    # Flags to be used for C-like compilers (C, C++, Obj-C)
    local clike_flags="$VLC_DEPLOYMENT_TARGET_CFLAG -arch $VLC_HOST_ARCH -isysroot $VLC_APPLE_SDK_PATH $1"

    local vlc_cppflags="-arch $VLC_HOST_ARCH -isysroot $VLC_APPLE_SDK_PATH"
    local vlc_cflags="$clike_flags"
    local vlc_cxxflags="$clike_flags"
    local vlc_objcflags="$clike_flags"

    # Vanilla clang doesn't use VLC_DEPLOYMENT_TAGET_LDFLAGS but only the CFLAGS variant
    local vlc_ldflags="$VLC_DEPLOYMENT_TARGET_LDFLAG $VLC_DEPLOYMENT_TARGET_CFLAG  -arch $VLC_HOST_ARCH"

    echo "Creating makefile..."
    test -e config.mak && unlink config.mak
    exec 3>config.mak || return $?

    printf '# This file was automatically generated!\n\n' >&3
    printf '%s := %s\n' "CPPFLAGS" "${vlc_cppflags}" >&3
    printf '%s := %s\n' "CFLAGS" "${vlc_cflags}" >&3
    printf '%s := %s\n' "CXXFLAGS" "${vlc_cxxflags}" >&3
    printf '%s := %s\n' "OBJCFLAGS" "${vlc_objcflags}" >&3
    printf '%s := %s\n' "LDFLAGS" "${vlc_ldflags}" >&3
    printf '%s := %s\n' "CC" "${VLC_HOST_CC}" >&3
    printf '%s := %s\n' "CPP" "${VLC_HOST_CPP}" >&3
    printf '%s := %s\n' "CXX" "${VLC_HOST_CXX}" >&3
    printf '%s := %s\n' "OBJC" "${VLC_HOST_OBJC}" >&3
    printf '%s := %s\n' "LD" "${VLC_HOST_LD}" >&3
    printf '%s := %s\n' "AR" "${VLC_HOST_AR}" >&3
    printf '%s := %s\n' "STRIP" "${VLC_HOST_STRIP}" >&3
    printf '%s := %s\n' "RANLIB" "${VLC_HOST_RANLIB}" >&3
    printf '%s := %s\n' "NM" "${VLC_HOST_NM}" >&3

    printf 'export %s=%s\n' "ACLOCAL_PATH" "$VLC_SRC_DIR/extras/tools/build/share/aclocal/" >&3

    # Add the ac_cv_ var exports in the config.mak for the contribs
    echo "Appending ac_cv_ vars to config.mak"
    vlcSetSymbolEnvironment ac_var_to_export_ac_var >&3
}

# Generate the source file with the needed array for
# the static VLC module list. This has to be compiled
# and linked into the static library
# Arguments:
#   Path of the output file
#   Array with module entry symbol names
gen_vlc_static_module_list()
{
    local output="$1"
    shift
    local symbol_array=( "$@" )
    touch "$output" || abort_err "Failure creating static module list file"

    local array_list
    local declarations_list

    for symbol in "${symbol_array[@]}"; do
        declarations_list+="VLC_ENTRY_FUNC(${symbol});\\n"
        array_list+="    ${symbol},\\n"
    done

    printf "\
#include <stddef.h>\\n\
#define VLC_ENTRY_FUNC(funcname)\
int funcname(int (*)(void *, void *, int, ...), void *)\\n\
%b\\n\
const void *vlc_static_modules[] = {\\n
%b
    NULL\\n
};" \
    "$declarations_list" "$array_list" >> "$output" \
      || abort_err "Failure writing static module list file"
}

##########################################################
#                  Main script logic                     #
##########################################################

# Parse arguments
while [ -n "$1" ]
do
    case "$1" in
        --help|-h)
            usage
            exit 0
            ;;
        --verbose)
            VLC_SCRIPT_VERBOSE=1
            VLC_USE_NUMBER_OF_CORES=1
            ;;
        --disable-debug)
            VLC_DISABLE_DEBUG=1
            ;;
        --enable-bitcode|--enable-bitcode=full)
            VLC_USE_BITCODE=1
            ;;
        --enable-bitcode=marker)
            VLC_USE_BITCODE=1
            VLC_BITCODE_FLAG="-fembed-bitcode-marker"
            ;;
        --enable-merge-plugins)
            VLC_MERGE_PLUGINS=1
            ;;
        --arch=*)
            VLC_HOST_ARCH="${1#--arch=}"
            ;;
        --sdk=*)
            VLC_APPLE_SDK_NAME="${1#--sdk=}"
            ;;
        --package-contribs)
            VLC_MAKE_PREBUILT_CONTRIBS=1
            ;;
        --with-prebuilt-contribs)
            VLC_USE_PREBUILT_CONTRIBS=1
            ;;
        --enable-shared)
            VLC_BUILD_DYNAMIC=1
            ;;
        --configure)
            VLC_CONFIGURE_ONLY=1
            ;;
        VLC_PREBUILT_CONTRIBS_URL=*)
            VLC_PREBUILT_CONTRIBS_URL="${1#VLC_PREBUILT_CONTRIBS_URL=}"
            ;;
        -j*)
            VLC_REQUESTED_CORE_COUNT=${1#-j}
            ;;
        *)
            echo >&2 "ERROR: Unrecognized option '$1'"
            usage
            exit 1
            ;;
    esac
    shift
done

export MAKEFLAGS="-j${VLC_USE_NUMBER_OF_CORES} ${MAKEFLAGS}"
if [ "${VLC_REQUESTED_CORE_COUNT}" != "0" ]; then
    export MAKEFLAGS="${MAKEFLAGS} -j${VLC_REQUESTED_CORE_COUNT}"
fi

# Check for some required tools before proceeding
check_tool xcrun

# TODO: Better command to get SDK name if none is set:
# xcodebuild -sdk $(xcrun --show-sdk-path) -version | awk -F '[()]' '{ print $2; exit; }'
# Aditionally a lot more is reported by this command, so this needs some more
# awk parsing or something to get other values with just one query.

# Validate given SDK name
validate_sdk_name "$VLC_APPLE_SDK_NAME"

# Validate architecture argument
validate_architecture "$VLC_HOST_ARCH"

# Set triplet (needs to be called after validating the arch)
set_host_triplet "$VLC_HOST_ARCH"
set_build_triplet

# Set pseudo-triplet
# FIXME: This should match the actual clang triplet and should be used for compiler invocation too!
readonly VLC_PSEUDO_TRIPLET="${VLC_HOST_ARCH}-apple-${VLC_HOST_PLATFORM}_${VLC_DEPLOYMENT_TARGET}"
# Contrib install dir
readonly VLC_CONTRIB_INSTALL_DIR="$VLC_BUILD_DIR/contrib/${VLC_HOST_ARCH}-${VLC_APPLE_SDK_NAME}"
# VLC install dir
readonly VLC_INSTALL_DIR="$VLC_BUILD_DIR/vlc-${VLC_APPLE_SDK_NAME}-${VLC_HOST_ARCH}"

echo "Build configuration"
echo "  Platform:         $VLC_HOST_PLATFORM"
echo "  Architecture:     $VLC_HOST_ARCH"
echo "  SDK Version:      $VLC_APPLE_SDK_VERSION"
echo "  Number of Cores:  $VLC_USE_NUMBER_OF_CORES"
if [ "$VLC_USE_BITCODE" -gt 0 ]; then
echo "  Bitcode:          enabled"
else
echo "  Bitcode:          disabled"
fi
echo ""

##########################################################
#                Prepare environment                     #
##########################################################

# Set PKG_CONFIG_LIBDIR to an empty string to prevent
# pkg-config from finding dependencies on the build
# machine, so that it only finds deps in contribs
export PKG_CONFIG_LIBDIR=""

# Add extras/tools to path
export PATH="$VLC_SRC_DIR/extras/tools/build/bin:$PATH"

# Do NOT set SDKROOT, as that is used by various Apple
# tools and clang and would lead to wrong results!
# Instead for now we set VLCSDKROOT which is needed
# to make the contrib script happy.
# TODO: Actually for macOS the contrib bootstrap script
# expects SDKROOT to be set, although we can't just do that
# due to the previously mentioned problem this causes.
export VLCSDKROOT="$VLC_APPLE_SDK_PATH"

# TODO: Adjust how that is handled in contrib script, to
# get rid of these env varibles that we need to set
if [ "$VLC_HOST_OS" = "ios" ]; then
    export BUILDFORIOS="yes"
elif [ "$VLC_HOST_OS" = "tvos" ]; then
    export BUILDFORIOS="yes"
    export BUILDFORTVOS="yes"
fi

# Default to "make" if there is no MAKE env variable
MAKE=${MAKE:-make}

# Attention! Do NOT use just "libtool" here and
# do NOT use the LIBTOOL env variable as this is
# expected to be Apple's libtool NOT GNU libtool!
APPL_LIBTOOL=$(xcrun -f libtool) \
  || abort_err "Failed to find Apple libtool with xcrun"


if [ "$VLC_CONFIGURE_ONLY" != "1" ]; then

##########################################################
#                 Extras tools build                     #
##########################################################

echo "Building needed tools (if missing)"

cd "$VLC_SRC_DIR/extras/tools" || abort_err "Failed cd to tools dir"
./bootstrap || abort_err "Bootstrapping tools failed"
$MAKE || abort_err "Building tools failed"
$MAKE .buildlibtool
echo ""

##########################################################
#                     Contribs build                     #
##########################################################
# Combine settings from config file
VLC_CONTRIB_OPTIONS=( "${VLC_CONTRIB_OPTIONS_BASE[@]}" )

if [ "$VLC_HOST_OS" = "macosx" ]; then
    VLC_CONTRIB_OPTIONS+=( "${VLC_CONTRIB_OPTIONS_MACOSX[@]}" )
elif [ "$VLC_HOST_OS" = "ios" ]; then
    VLC_CONTRIB_OPTIONS+=( "${VLC_CONTRIB_OPTIONS_IOS[@]}" )
elif [ "$VLC_HOST_OS" = "tvos" ]; then
    VLC_CONTRIB_OPTIONS+=( "${VLC_CONTRIB_OPTIONS_TVOS[@]}" )
fi

# Create dir to build contribs in
cd "$VLC_SRC_DIR/contrib" || abort_err "Failed cd to contrib dir"
mkdir -p "contrib-$VLC_PSEUDO_TRIPLET"
cd "contrib-$VLC_PSEUDO_TRIPLET" || abort_err "Failed cd to contrib build dir"

# Create contrib install dir if it does not already exist
mkdir -p "$VLC_CONTRIB_INSTALL_DIR"

# Write config.mak with flags for the build and compiler overrides
# Set flag to error on partial availability
write_config_mak "-Werror=partial-availability"

if [ "$VLC_USE_BITCODE" -gt "0" ]; then
    VLC_CONTRIB_OPTIONS+=( "--enable-bitcode" )
fi

# Bootstrap contribs
../bootstrap \
    --host="$VLC_HOST_TRIPLET" \
    --build="$VLC_BUILD_TRIPLET" \
    --prefix="$VLC_CONTRIB_INSTALL_DIR" \
    "${VLC_CONTRIB_OPTIONS[@]}" \
|| abort_err "Bootstrapping contribs failed"

# Print list of contribs that will be built
$MAKE list

if [ "$VLC_USE_PREBUILT_CONTRIBS" -gt "0" ]; then
    echo "Fetching prebuilt contribs"
    # Fetch prebuilt contribs
    if [ -z "$VLC_PREBUILT_CONTRIBS_URL" ]; then
        $MAKE prebuilt || PREBUILT_FAILED=yes && echo "ERROR: Fetching prebuilt contribs failed" >&2
    else
        $MAKE prebuilt PREBUILT_URL="$VLC_PREBUILT_CONTRIBS_URL" \
             || PREBUILT_FAILED=yes && echo "ERROR: Fetching prebuilt contribs from ${VLC_PREBUILT_CONTRIBS_URL} failed" >&2
    fi
else
    PREBUILT_FAILED=yes
fi
if [ -n "$PREBUILT_FAILED" ]; then
    echo "Building contribs for $VLC_HOST_ARCH"
    # Download source packages
    $MAKE fetch

    # Build contribs
    $MAKE || abort_err "Building contribs failed"

    # Make prebuilt contribs package
    if [ "$VLC_MAKE_PREBUILT_CONTRIBS" -gt "0" ]; then
        $MAKE package || abort_err "Creating prebuilt contribs package failed"
    fi
else
    $MAKE tools
fi

echo ""
fi

##########################################################
#                      VLC build                         #
##########################################################

echo "Building VLC for $VLC_HOST_ARCH"

# Set flags for VLC build
set_host_envvars "-g"

# Combine settings from config file
VLC_CONFIG_OPTIONS=( "${VLC_CONFIG_OPTIONS_BASE[@]}" )

if [ "$VLC_HOST_OS" = "macosx" ]; then
    VLC_CONFIG_OPTIONS+=( "${VLC_CONFIG_OPTIONS_MACOSX[@]}" )
elif [ "$VLC_HOST_OS" = "ios" ]; then
    VLC_CONFIG_OPTIONS+=( "${VLC_CONFIG_OPTIONS_IOS[@]}" )
elif [ "$VLC_HOST_OS" = "tvos" ]; then
    VLC_CONFIG_OPTIONS+=( "${VLC_CONFIG_OPTIONS_TVOS[@]}" )
fi

if [ "$VLC_DISABLE_DEBUG" -gt "0" ]; then
    VLC_CONFIG_OPTIONS+=( "--disable-debug" )
fi

#if [ "$VLC_BUILD_DYNAMIC" -gt "0" ]; then
#    VLC_CONFIG_OPTIONS+=( "--enable-shared" )
#else
#    VLC_CONFIG_OPTIONS+=( "--disable-shared" "--enable-static" )
#fi
VLC_CONFIG_OPTIONS+=( "--enable-static-libvlc" )

if [ "$VLC_MERGE_PLUGINS" -gt "0" ]; then
    VLC_CONFIG_OPTIONS+=( "--enable-merge-plugins" )
fi

# Bootstrap VLC
cd "$VLC_SRC_DIR" || abort_err "Failed cd to VLC source dir"
if ! [ -e configure ]; then
    echo "Bootstraping vlc"
    ./bootstrap
fi

# Build
mkdir -p "${VLC_BUILD_DIR}/build"
cd "${VLC_BUILD_DIR}/build" || abort_err "Failed cd to VLC build dir"

echo "#!/usr/bin/env sh" > ar.sh
echo "export CFLAGS=\"${CFLAGS}\"" >> ar.sh
echo "export LDFLAGS=\"${LDFLAGS}\"" >> ar.sh
echo "export CC=\"${VLC_HOST_CC}\"" >> ar.sh
echo "export AR=\"${VLC_HOST_AR}\"" >> ar.sh
echo "${VLC_SCRIPT_DIR}/ar.sh \$@" >> ar.sh
chmod +x ar.sh

# Create VLC install dir if it does not already exist
mkdir -p "$VLC_INSTALL_DIR"

vlcSetSymbolEnvironment \
hostenv ../../configure \
    --with-contrib="$VLC_CONTRIB_INSTALL_DIR" \
    --host="$VLC_HOST_TRIPLET" \
    --build="$VLC_BUILD_TRIPLET" \
    --prefix="$VLC_INSTALL_DIR" \
    "${VLC_CONFIG_OPTIONS[@]}" \
 || abort_err "Configuring VLC failed"

if [ "$VLC_CONFIGURE_ONLY" != 1 ]; then
$MAKE || abort_err "Building VLC failed"

$MAKE install || abort_err "Installing VLC failed"

echo ""
echo "Build succeeded!"
fi
