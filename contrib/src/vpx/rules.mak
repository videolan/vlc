# libvpx

VPX_VERSION := v1.1.0
VPX_URL := http://webm.googlecode.com/files/libvpx-$(VPX_VERSION).tar.bz2

$(TARBALLS)/libvpx-$(VPX_VERSION).tar.bz2:
	$(call download,$(VPX_URL))

.sum-vpx: libvpx-$(VPX_VERSION).tar.bz2

libvpx: libvpx-$(VPX_VERSION).tar.bz2 .sum-vpx
	$(UNPACK)
	$(APPLY) $(SRC)/vpx/libvpx-no-cross.patch
	$(APPLY) $(SRC)/vpx/libvpx-no-abi.patch
	$(APPLY) $(SRC)/vpx/windows.patch
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/vpx/libvpx-mac.patch
	$(APPLY) $(SRC)/vpx/libvpx-mac-mountain-lion.patch
endif
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/vpx/libvpx-win32.patch
endif
ifneq ($(which bash),/bin/bash)
	sed -i.orig \
		s,^\#!/bin/bash,\#!`which bash`,g \
		`grep -Rl ^\#!/bin/bash libvpx-$(VPX_VERSION)`
endif
	$(MOVE)

DEPS_vpx =

ifdef HAVE_CROSS_COMPILE
VPX_CROSS := $(HOST)-
else
VPX_CROSS :=
endif

ifeq ($(ARCH),arm)
VPX_ARCH := armv7
else ifeq ($(ARCH),i386)
VPX_ARCH := x86
else ifeq ($(ARCH),mips)
VPX_ARCH := mips32
else ifeq ($(ARCH),ppc)
VPX_ARCH := ppc32
else ifeq ($(ARCH),ppc64)
VPX_ARCH := ppc64
else ifeq ($(ARCH),sparc)
VPX_ARCH := sparc
else ifeq ($(ARCH),x86_64)
VPX_ARCH := x86_64
endif

ifdef HAVE_LINUX
VPX_OS := linux
else ifdef HAVE_DARWIN_OS
ifeq ($(ARCH),arm)
VPX_OS := darwin
else
ifeq ($(OSX_VERSION),10.5)
VPX_OS := darwin9
else
VPX_OS := darwin10
endif
endif
else ifdef HAVE_SOLARIS
VPX_OS := solaris
else ifdef HAVE_WIN64 # must be before WIN32
VPX_OS := win64
else ifdef HAVE_WIN32
VPX_OS := win32
else ifdef HAVE_BSD
VPX_OS := linux
endif

VPX_TARGET := generic-gnu
ifdef VPX_ARCH
ifdef VPX_OS
VPX_TARGET := $(VPX_ARCH)-$(VPX_OS)-gcc
endif
endif

VPX_CONF := \
	--enable-runtime-cpu-detect \
	--disable-install-bins \
	--disable-install-srcs \
	--disable-install-libs \
	--disable-install-docs \
	--disable-examples \
	--disable-vp8-decoder
ifndef HAVE_WIN32
VPX_CONF += --enable-pic
endif
ifdef HAVE_MACOSX
VPX_CONF += --sdk-path=$(MACOSX_SDK)
endif
ifdef HAVE_IOS
VPX_CONF += --sdk-path=$(SDKROOT)
endif

.vpx: libvpx
	cd $< && CROSS=$(VPX_CROSS) ./configure --target=$(VPX_TARGET) \
		$(VPX_CONF)
	cd $< && $(MAKE) install
	rm -Rf -- "$(PREFIX)/include/vpx/"
	mkdir -p -- "$(PREFIX)/include/vpx/"
	# Of course! Why the hell would it be listed or in make install?
	cp $</vpx/*.h $</vpx_ports/*.h "$(PREFIX)/include/vpx/"
	rm -f -- "$(PREFIX)/include/vpx/config.h"
	$(RANLIB) $</libvpx.a
	# Of course! Why the hell would it be listed or in make install?
	mkdir -p -- "$(PREFIX)/lib"
	install -- $</libvpx.a "$(PREFIX)/lib/libvpx.a"
	touch $@
