#!/usr/bin/env bash
# Copyright (C) Marvin Scholz
#
# Script to help build VLC or libVLC for Apple OSes
# Supported OSes: macOS, tvOS, macOS
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
# The number of cores to compile on, or 0 + 1 if not darwin
CORE_COUNT=$(sysctl -n machdep.cpu.core_count || echo 0)
let VLC_USE_NUMBER_OF_CORES=$CORE_COUNT+1
# whether to disable debug mode (the default) or not
VLC_DISABLE_DEBUG=0
# whether to compile with bitcode or not
VLC_USE_BITCODE=0
# whether to build static or dynamic plugins
VLC_BUILD_DYNAMIC=0

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
    echo " --enable-bitcode Enable bitcode for compilation"
    echo " --disable-debug  Disable libvlc debug mode (for release)"
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
    if [ "$VLC_USE_BITCODE" -gt "0" ]; then
        clike_flags+=" -fembed-bitcode"
    fi

    export CPPFLAGS="-arch $VLC_HOST_ARCH -isysroot $VLC_APPLE_SDK_PATH"

    export CFLAGS="$clike_flags"
    export CXXFLAGS="$clike_flags"
    export OBJCFLAGS="$clike_flags"

    # Vanilla clang doesn't use VLC_DEPLOYMENT_TAGET_LDFLAGS but only the CFLAGS variant
    export LDFLAGS="$VLC_DEPLOYMENT_TARGET_LDFLAG $VLC_DEPLOYMENT_TARGET_CFLAG -arch $VLC_HOST_ARCH"
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
    if [ "$VLC_USE_BITCODE" -gt "0" ]; then
        clike_flags+=" -fembed-bitcode"
    fi

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
        --enable-bitcode)
            VLC_USE_BITCODE=1
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
        VLC_PREBUILT_CONTRIBS_URL=*)
            VLC_PREBUILT_CONTRIBS_URL="${1#VLC_PREBUILT_CONTRIBS_URL=}"
            ;;
        -j*)
            VLC_USE_NUMBER_OF_CORES=${1#-j}
            ;;
        *)
            echo >&2 "ERROR: Unrecognized option '$1'"
            usage
            exit 1
            ;;
    esac
    shift
done

# Validate arguments
if [ "$VLC_MAKE_PREBUILT_CONTRIBS" -gt "0" ] &&
   [ "$VLC_USE_PREBUILT_CONTRIBS" -gt "0" ]; then
    echo >&2 "ERROR: The --package-contribs and --with-prebuilt-contribs options"
    echo >&2 "       can not be used together."
    usage
    exit 1
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

##########################################################
#                 Extras tools build                     #
##########################################################

echo "Building needed tools (if missing)"

cd "$VLC_SRC_DIR/extras/tools" || abort_err "Failed cd to tools dir"
./bootstrap || abort_err "Bootstrapping tools failed"
$MAKE -j$VLC_USE_NUMBER_OF_CORES || abort_err "Building tools failed"
if [ $VLC_HOST_ARCH = "armv7" ]; then
$MAKE -j$VLC_USE_NUMBER_OF_CORES .buildgas \
    || abort_err "Building gas-preprocessor tool failed"
fi
echo ""

##########################################################
#                     Contribs build                     #
##########################################################
if [ "$VLC_USE_PREBUILT_CONTRIBS" -gt "0" ]; then
    echo "Fetching prebuilt contribs"
else
    echo "Building contribs for $VLC_HOST_ARCH"
fi

# Set symbol blacklist for autoconf
vlcSetSymbolEnvironment > /dev/null

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

# Bootstrap contribs
../bootstrap \
    --host="$VLC_HOST_TRIPLET" \
    --prefix="$VLC_CONTRIB_INSTALL_DIR" \
    "${VLC_CONTRIB_OPTIONS[@]}" \
|| abort_err "Bootstrapping contribs failed"

if [ "$VLC_USE_PREBUILT_CONTRIBS" -gt "0" ]; then
    # Fetch prebuilt contribs
    if [ -z "$VLC_PREBUILT_CONTRIBS_URL" ]; then
        $MAKE prebuilt || abort_err "Fetching prebuilt contribs failed"
    else
        $MAKE prebuilt PREBUILT_URL="$VLC_PREBUILT_CONTRIBS_URL" \
            || abort_err "Fetching prebuilt contribs from ${VLC_PREBUILT_CONTRIBS_URL} failed"
    fi
else
    # Print list of contribs that will be built
    $MAKE list

    # Download source packages
    $MAKE fetch -j$VLC_USE_NUMBER_OF_CORES

    # Build contribs
    $MAKE -j$VLC_USE_NUMBER_OF_CORES || abort_err "Building contribs failed"

    # Make prebuilt contribs package
    if [ "$VLC_MAKE_PREBUILT_CONTRIBS" -gt "0" ]; then
        $MAKE package || abort_err "Creating prebuilt contribs package failed"
    fi
fi

echo ""

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

if [ "$VLC_BUILD_DYNAMIC" -gt "0" ]; then
    VLC_CONFIG_OPTIONS+=( "--enable-shared" )
else
    VLC_CONFIG_OPTIONS+=( "--disable-shared" "--enable-static" )
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

# Create VLC install dir if it does not already exist
mkdir -p "$VLC_INSTALL_DIR"

hostenv ../../configure \
    --with-contrib="$VLC_CONTRIB_INSTALL_DIR" \
    --host="$VLC_HOST_TRIPLET" \
    --prefix="$VLC_INSTALL_DIR" \
    "${VLC_CONFIG_OPTIONS[@]}" \
 || abort_err "Configuring VLC failed"

$MAKE -j$VLC_USE_NUMBER_OF_CORES || abort_err "Building VLC failed"

$MAKE install || abort_err "Installing VLC failed"

echo ""
# Shortcut the build of the static bundle when using the dynamic loader
if [ "$VLC_BUILD_DYNAMIC" -gt "0" ]; then
    echo "Build succeeded!"
    exit 0
fi

##########################################################
#                 Remove unused modules                  #
##########################################################

echo "Removing modules that are on the removal list"

# Combine settings from config file
VLC_MODULE_REMOVAL_LIST=( "${VLC_MODULE_REMOVAL_LIST_BASE[@]}" )

if [ "$VLC_HOST_OS" = "macosx" ]; then
    VLC_MODULE_REMOVAL_LIST+=( "${VLC_MODULE_REMOVAL_LIST_MACOSX[@]}" )
elif [ "$VLC_HOST_OS" = "ios" ]; then
    VLC_MODULE_REMOVAL_LIST+=( "${VLC_MODULE_REMOVAL_LIST_IOS[@]}" )
elif [ "$VLC_HOST_OS" = "tvos" ]; then
    VLC_MODULE_REMOVAL_LIST+=( "${VLC_MODULE_REMOVAL_LIST_TVOS[@]}" )
fi

for module in "${VLC_MODULE_REMOVAL_LIST[@]}"; do
    find "$VLC_INSTALL_DIR/lib/vlc/plugins" \
        -name "lib${module}_plugin.a" \
        -type f \
        -exec rm '{}' \;
done

echo ""

##########################################################
#        Compile object with static module list          #
##########################################################

echo "Compile VLC static modules list object"

mkdir -p "${VLC_BUILD_DIR}/static-lib"
cd "${VLC_BUILD_DIR}/static-lib" \
 || abort_err "Failed cd to VLC static-lib build dir"

# Collect paths of all static libraries needed (plugins and contribs)
VLC_STATIC_FILELIST_NAME="static-libs-list"
rm -f "$VLC_STATIC_FILELIST_NAME"
touch "$VLC_STATIC_FILELIST_NAME"

VLC_PLUGINS_SYMBOL_LIST=()

# Find all static plugins in build dir
while IFS=  read -r -d $'\0' plugin_path; do
    # Get module entry point symbol name (_vlc_entry__MODULEFULLNAME)
    nm_symbol_output=( $(${VLC_HOST_NM} "$plugin_path" | grep _vlc_entry__) ) \
      || abort_err "Failed to find module entry function in '$plugin_path'"

    symbol_name="${nm_symbol_output[2]:1}"
    VLC_PLUGINS_SYMBOL_LIST+=( "$symbol_name" )

    echo "$plugin_path" >> "$VLC_STATIC_FILELIST_NAME"

done < <(find "$VLC_INSTALL_DIR/lib/vlc/plugins" -name "*.a" -print0)

# Generate code with module list
VLC_STATIC_MODULELIST_NAME="static-module-list"
rm -f "${VLC_STATIC_MODULELIST_NAME}.c" "${VLC_STATIC_MODULELIST_NAME}.o"
gen_vlc_static_module_list "${VLC_STATIC_MODULELIST_NAME}.c" "${VLC_PLUGINS_SYMBOL_LIST[@]}"

${VLC_HOST_CC:-cc} -c  ${CFLAGS} "${VLC_STATIC_MODULELIST_NAME}.c" \
  || abort_err "Compiling module list file failed"

echo "${VLC_BUILD_DIR}/static-lib/${VLC_STATIC_MODULELIST_NAME}.o" \
  >> "$VLC_STATIC_FILELIST_NAME"

echo ""

##########################################################
#          Link together full static library             #
##########################################################

echo "Linking VLC modules and contribs statically"

echo "$VLC_INSTALL_DIR/lib/libvlc.a" >> "$VLC_STATIC_FILELIST_NAME"
echo "$VLC_INSTALL_DIR/lib/libvlccore.a" >> "$VLC_STATIC_FILELIST_NAME"
echo "$VLC_INSTALL_DIR/lib/vlc/libcompat.a" >> "$VLC_STATIC_FILELIST_NAME"

# Find all static contribs in build dir
find "$VLC_CONTRIB_INSTALL_DIR/lib" -name '*.a' -print >> "$VLC_STATIC_FILELIST_NAME" \
  || abort_err "Failed finding installed static contribs in '$VLC_CONTRIB_INSTALL_DIR/lib'"

# Link static libs together using libtool
$APPL_LIBTOOL -static \
    -no_warning_for_no_symbols \
    -filelist "$VLC_STATIC_FILELIST_NAME" \
    -o "libvlc-full-static.a" \
  || abort_err "Failed running Apple libtool to combine static libraries together"

echo ""
echo "Build succeeded!"
