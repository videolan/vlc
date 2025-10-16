#!/bin/sh
# SPDX-License-Identifier: ISC
# Copyright (C) 2024 VideoLabs, VLC authors and VideoLAN
#
# Authors: Denis Charmet <typx@dinauz.org>
#          Steve Lhomme <robux4@videolabs.io>
#          Alexandre Janniaux <ajanni@videolabs.io>
#
# Transform a compilation triplet from autoconf to cargo/llvm dialect
# based on https://doc.rust-lang.org/rustc/platform-support.html

DARWIN=
ARCH=
OS=
LLVM=
UWP=
SIMULATOR=
TRIPLET=

abort_err()
{
    echo "ERROR: $1" >&2
    exit 1
}

return_triplet()
{
  echo "$1"
  exit 0
}

validate_darwin()
{
  D=$(echo $1 | cut -f2 -d"=")
  case $D in
    macos)
      DARWIN=macos
      ;;
    ios)
      DARWIN=ios
      ;;
    tvos)
      DARWIN=tvos
      ;;
    watchos)
      DARWIN=watchos
      ;;
    xros)
      DARWIN=xros
      ;;
    *)
      abort_err "Unsupported Darwin variant '$D'"
      ;;
  esac
}

validate_triplet()
{
  TRIPLET=$1
  ARCH=$(echo $1 | cut -f 1 -d '-')
  UNUSED=$(echo $1 | cut -f 2 -d '-')
  OS=$(echo $1 | cut -f 3 -d '-')
  REST=$(echo $1 | cut -f 4 -d '-')

  if test -n "$REST"; then
    OS=$REST
  fi
  if test -z "$ARCH" || test -z "$UNUSED" || test -z "$OS"; then
    abort_err "Unsupported triplet '$1'"
  fi
}

print_usage()
{
    echo "Usage: $0 [--uwp] [--llvm] [--darwin {macos,ios,tvos,watchos,xros}] [--simulator] triplet"
}


for ARG in "$@"; do
  case $ARG in
    --uwp)
      UWP=1
      ;;
    --llvm)
      LLVM=1
      ;;
    --simulator)
      SIMULATOR=1
      ;;
    --darwin=*)
      validate_darwin $ARG
      ;;
    *-*-*)
      validate_triplet $ARG
      break
      ;;
    *)
      print_usage
      abort_err "Unknown parameter $ARG"
      ;;
  esac
done

case $OS in
  mingw32|mingw32ucrt|mingw32uwp)
    if test -n "$UWP"; then
      abort_err "UWP Windows is Tier 3"
    fi
    case $ARCH in
      armv7)
        abort_err "ARMv7 Windows not supported by Rust"
        ;;
    esac
    if test -n "$LLVM"; then
      return_triplet $ARCH-pc-windows-gnullvm
    else
      return_triplet $ARCH-pc-windows-gnu
    fi
    ;;

  android)
    case $ARCH in
      aarch64|i686|x86_64)
        return_triplet "$ARCH-linux-android"
        ;;
    esac
    ;;

  androideabi)
    case $ARCH in
      arm|armv7|thumbv7neon)
        return_triplet "arm-linux-androideabi"
        ;;
    esac
    ;;

  darwin*)
    case $DARWIN in
      macos)
        case $ARCH in
          aarch64|arm64)
            return_triplet aarch64-apple-darwin
            ;;
          x86_64)
            return_triplet x86_64-apple-darwin
            ;;
        esac
        ;;

      ios)
        if test -n "$SIMULATOR"; then
          case $ARCH in
            aarch64|arm64)
              return_triplet aarch64-apple-ios-sim
              ;;
          esac
        else
          case $ARCH in
            aarch64|arm64)
              return_triplet aarch64-apple-ios
              ;;
            x86_64)
              return_triplet x86_64-apple-ios
              ;;
          esac
        fi
        ;;
      tvos)
        if test -z "$SIMULATOR"; then
          case $ARCH in
            aarch64|arm64)
              return_triplet aarch64-apple-tvos
              ;;
           esac
        fi
        ;;
      watchos)
        if test -z "$SIMULATOR"; then
          case $ARCH in
            aarch64|arm64)
              return_triplet aarch64-apple-watchos
              ;;
            arm64_32)
              return_triplet arm64_32-apple-watchos
              ;;
           esac
        fi
        ;;
      xros)
        if test -z "$SIMULATOR"; then
          case $ARCH in
            aarch64|arm64)
              return_triplet aarch64-apple-visionos
              ;;
           esac
        fi
        ;;

    esac
    abort_err "Unsupported Darwin triplet '$TRIPLET' for '$DARWIN'"
    ;;

  gnueabihf)
    case $ARCH in
      arm|armv7|thumbv7neon)
        return_triplet $ARCH-unknown-linux-gnueabihf
        ;;
    esac
    ;;

  gnu)
    case $ARCH in
      riscv64)
        return_triplet riscv64gc-unknown-linux-gnu
        ;;
      x86_64|aarch64|i686|loongarch64|powerpc|powerpc64|powerpcle|s390x|i586|sparc64)
        return_triplet $ARCH-unknown-linux-gnu
        ;;
    esac
    ;;

  netbsd)
    case $ARCH in
      x86_64)
        return_triplet x86_64-unknown-netbsd
        ;;
    esac
    ;;

  freebsd)
    case $ARCH in
      x86_64)
        return_triplet x86_64-unknown-freebsd
        ;;
    esac
    ;;

  emscripten)
    case $ARCH in
      wasm32)
        return_triplet wasm32-unknown-emscripten
        ;;
    esac
    ;;

  *)
    abort_err "Unknown OS '$OS'"
    ;;
esac

abort_err "Unsupported triplet '$TRIPLET'"
