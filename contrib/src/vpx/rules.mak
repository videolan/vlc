# libvpx

VPX_VERSION := 1.7.0
VPX_URL := http://github.com/webmproject/libvpx/archive/v${VPX_VERSION}.tar.gz

PKGS += vpx
ifeq ($(call need_pkg,"vpx >= 1.5.0"),)
PKGS_FOUND += vpx
endif

$(TARBALLS)/libvpx-$(VPX_VERSION).tar.gz:
	$(call download_pkg,$(VPX_URL),vpx)

.sum-vpx: libvpx-$(VPX_VERSION).tar.gz

libvpx: libvpx-$(VPX_VERSION).tar.gz .sum-vpx
	$(UNPACK)
	$(APPLY) $(SRC)/vpx/libvpx-mac.patch
	$(APPLY) $(SRC)/vpx/libvpx-ios.patch
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/vpx/libvpx-android.patch
	$(APPLY) $(SRC)/vpx/libvpx-android-fix_cortex_a8-flag.patch
	$(APPLY) $(SRC)/vpx/libvpx-android-toolchain_path.patch
endif
	$(APPLY) $(SRC)/vpx/0001-ads2gas-Add-a-noelf-option.patch
	$(APPLY) $(SRC)/vpx/0002-configure-Add-an-armv7-win32-gcc-target.patch
	$(APPLY) $(SRC)/vpx/0003-configure-Add-an-arm64-win64-gcc-target.patch
	$(MOVE)

DEPS_vpx =

ifdef HAVE_WIN32
DEPS_vpx += pthreads $(DEPS_pthreads)
endif

ifdef HAVE_CROSS_COMPILE
VPX_CROSS := $(HOST)-
else
VPX_CROSS :=
endif

VPX_LDFLAGS := $(LDFLAGS)

ifeq ($(ARCH),arm)
ifdef HAVE_IOS
ifneq ($(filter armv7s%,$(subst -, ,$(HOST))),)
VPX_ARCH := armv7s
else
VPX_ARCH := armv7
endif
else
VPX_ARCH := armv7
endif
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
else ifeq ($(ARCH),aarch64)
VPX_ARCH := arm64
endif

ifdef HAVE_ANDROID
VPX_OS := android
else ifdef HAVE_LINUX
VPX_OS := linux
else ifdef HAVE_MACOSX
VPX_OS := darwin10
VPX_CROSS :=
else ifdef HAVE_IOS
ifeq ($(ARCH),$(filter $(ARCH), arm aarch64))
VPX_OS := darwin
else
VPX_OS := darwin11
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
	--disable-docs \
	--disable-examples \
	--disable-unit-tests \
	--disable-install-bins \
	--disable-install-docs \
	--disable-dependency-tracking \
	--enable-vp9-highbitdepth

ifndef HAVE_WIN32
ifndef HAVE_IOS
VPX_CONF += --enable-runtime-cpu-detect
endif
else
# WIN32
ifeq ($(filter arm aarch64, $(ARCH)),)
# Only enable runtime cpu detect on architectures other than arm/aarch64
VPX_CONF += --enable-runtime-cpu-detect
endif
endif

ifndef BUILD_ENCODERS
VPX_CONF += --disable-vp8-encoder --disable-vp9-encoder
endif

ifndef HAVE_WIN32
VPX_CONF += --enable-pic
else
VPX_CONF += --extra-cflags="-mstackrealign"
endif
ifdef HAVE_MACOSX
VPX_CONF += --sdk-path=$(MACOSX_SDK) --extra-cflags="$(EXTRA_CFLAGS)"
endif
ifdef HAVE_IOS
VPX_CONF += --sdk-path=$(IOS_SDK) --enable-vp8-decoder
ifdef HAVE_TVOS
VPX_LDFLAGS := -L$(IOS_SDK)/usr/lib -isysroot $(IOS_SDK) -mtvos-version-min=9.0
else
VPX_LDFLAGS := -L$(IOS_SDK)/usr/lib -isysroot $(IOS_SDK) -miphoneos-version-min=8.4
endif
ifeq ($(ARCH),aarch64)
VPX_LDFLAGS += -arch arm64
else
ifndef HAVE_IOS
VPX_LDFLAGS += -arch $(ARCH)
endif
endif
endif
ifdef HAVE_ANDROID
# vpx configure.sh overrides our sysroot and it looks for it itself, and
# uses that path to look for the compiler (which we already know)
VPX_CONF += --sdk-path=$(shell dirname $(shell which $(HOST)-clang))
endif

ifneq ($(filter i386 x86_64,$(ARCH)),)
# broken text relocations or invalid register for .seh_savexmm with gcc8
VPX_CONF += --disable-mmx
endif

ifndef WITH_OPTIMIZATION
VPX_CONF += --enable-debug --disable-optimizations
endif

.vpx: libvpx
	cd $< && LDFLAGS="$(VPX_LDFLAGS)" CROSS=$(VPX_CROSS) ./configure --target=$(VPX_TARGET) \
		$(VPX_CONF) --prefix=$(PREFIX)
	cd $< && $(MAKE)
	cd $< && ../../../contrib/src/pkg-static.sh vpx.pc
	cd $< && $(MAKE) install
	touch $@
