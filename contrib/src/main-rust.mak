# Cargo/Rust specific makefile rules for VLC 3rd party libraries ("contrib")
# Copyright (C) 2003-2020 the VideoLAN team
#
# This file is under the same license as the vlc package.

ifdef HAVE_WIN32
ifeq ($(HOST),i686-w64-mingw32)
RUST_TARGET = i686-pc-windows-gnu # ARCH is i386
else
RUST_TARGET = $(ARCH)-pc-windows-gnu
endif
else ifdef HAVE_ANDROID
RUST_TARGET = $(HOST)
else ifdef HAVE_IOS
ifneq ($(ARCH),arm) # iOS 32bit is Tier 3
ifndef HAVE_TVOS # tvOS is Tier 3
RUST_TARGET = $(ARCH)-apple-ios
endif
endif
else ifdef HAVE_MACOSX
RUST_TARGET = $(ARCH)-apple-darwin
else ifdef HAVE_SOLARIS
RUST_TARGET = x86_64-sun-solaris
else ifdef HAVE_LINUX
ifeq ($(HOST),arm-linux-gnueabihf)
RUST_TARGET = arm-unknown-linux-gnueabihf #add eabihf
else
RUST_TARGET = $(ARCH)-unknown-linux-gnu
endif
else ifdef HAVE_BSD
RUST_TARGET = $(HOST)
endif

# For now, VLC don't support Tier 3 platforms (ios 32bit, tvOS).
# Supporting a Tier 3 platform means building an untested rust toolchain.
# TODO Let's hope tvOS move from Tier 3 to Tier 2 before the VLC 4.0 release.
ifneq ($(RUST_TARGET),)
BUILD_RUST="1"
endif
