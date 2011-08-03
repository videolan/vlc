# libvpx

VPX_VERSION := v0.9.6
VPX_URL := http://webm.googlecode.com/files/libvpx-$(VPX_VERSION).tar.bz2

$(TARBALLS)/libvpx-$(VPX_VERSION).tar.bz2:
	$(call download,$(VPX_URL))

.sum-vpx: libvpx-$(VPX_VERSION).tar.bz2

libvpx: libvpx-$(VPX_VERSION).tar.bz2 .sum-vpx
	$(UNPACK)
	$(APPLY) $(SRC)/vpx/libvpx-no-cross.patch
	$(APPLY) $(SRC)/vpx/libvpx-no-abi.patch
	$(APPLY) $(SRC)/vpx/libvpx-win64.patch
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
#DEPS_vpx += yasm $(DEPS_yasm)
else ifeq ($(ARCH),mips)
VPX_ARCH := mips32
else ifeq ($(ARCH),ppc)
VPX_ARCH := ppc32
else ifeq ($(ARCH),ppc64)
VPX_ARCH := ppc64
else ifeq ($(ARCH),sparc)
VPX_ARCH := sparch
else ifeq ($(ARCH),x86_64)
VPX_ARCH := x86_64
#DEPS_vpx += yasm $(DEPS_yasm)
endif

ifdef HAVE_LINUX
VPX_OS := linux
else ifdef HAVE_MACOSX
ifeq ($(ARCH),arm)
VPX_OS := darwin
else
VPX_OS := darwin9
endif
else ifdef HAVE_SOLARIS
VPX_OS := solaris
else ifdef HAVE_WIN64 # must be before WIN32
VPX_OS := win64
else ifdef HAVE_WIN32
VPX_OS := win32
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
	install -D -- $</libvpx.a "$(PREFIX)/lib/libvpx.a"
	touch $@
