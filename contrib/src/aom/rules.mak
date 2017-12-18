# aom
AOM_VERSION := git
AOM_HASH := HEAD
AOM_GITURL := https://aomedia.googlesource.com/aom/+archive/$(AOM_HASH).tar.gz

# Default disabled for now
# PKGS += aom
ifeq ($(call need_pkg,"aom"),)
PKGS_FOUND += aom
endif

$(TARBALLS)/aom-$(AOM_VERSION).tar.gz:
	$(call download,$(AOM_GITURL))

.sum-aom: aom-$(AOM_VERSION).tar.gz
	$(warning $@ not implemented)
	touch $@

aom: aom-$(AOM_VERSION).tar.gz .sum-aom
	rm -Rf $@-$(AOM_VERSION) $@
	mkdir -p $@-$(AOM_VERSION)
	tar xvzf "$<" -C $@-$(AOM_VERSION)
	$(MOVE)

DEPS_aom =

ifdef HAVE_CROSS_COMPILE
AOM_CROSS := $(HOST)-
else
AOM_CROSS :=
endif

AOM_LDFLAGS := $(LDFLAGS)

ifeq ($(ARCH),arm)
AOM_ARCH := armv7
else ifeq ($(ARCH),i386)
AOM_ARCH := x86
else ifeq ($(ARCH),mips)
AOM_ARCH := mips32
else ifeq ($(ARCH),ppc)
AOM_ARCH := ppc32
else ifeq ($(ARCH),ppc64)
AOM_ARCH := ppc64
else ifeq ($(ARCH),sparc)
AOM_ARCH := sparc
else ifeq ($(ARCH),x86_64)
AOM_ARCH := x86_64
endif

ifdef HAVE_ANDROID
AOM_OS := android
else ifdef HAVE_LINUX
AOM_OS := linux
else ifdef HAVE_MACOSX
ifeq ($(OSX_VERSION),10.5)
AOM_OS := darwin9
else
AOM_OS := darwin10
endif
else ifdef HAVE_IOS
AOM_OS := darwin11
else ifdef HAVE_SOLARIS
AOM_OS := solaris
else ifdef HAVE_WIN64 # must be before WIN32
AOM_OS := win64
else ifdef HAVE_WIN32
AOM_OS := win32
else ifdef HAVE_BSD
AOM_OS := linux
endif

AOM_TARGET := generic-gnu
ifdef AOM_ARCH
ifdef AOM_OS
AOM_TARGET := $(AOM_ARCH)-$(AOM_OS)-gcc
endif
endif

AOM_CONF := \
	--enable-runtime-cpu-detect \
	--disable-docs \
	--disable-examples \
	--disable-unit-tests \
	--disable-install-bins \
	--disable-install-docs \
	--disable-dependency-tracking

ifndef BUILD_ENCODERS
AOM_CONF += --disable-av1-encoder
endif

ifndef HAVE_WIN32
AOM_CONF += --enable-pic
endif
ifdef HAVE_MACOSX
AOM_CONF += --sdk-path=$(MACOSX_SDK)
endif
ifdef HAVE_IOS
AOM_CONF += --sdk-path=$(IOS_SDK)
ifdef HAVE_TVOS
AOM_LDFLAGS := -L$(IOS_SDK)/usr/lib -isysroot $(IOS_SDK) -mtvos-version-min=9.0
else
AOM_LDFLAGS := -L$(IOS_SDK)/usr/lib -isysroot $(IOS_SDK) -miphoneos-version-min=6.1
endif
ifeq ($(ARCH),aarch64)
AOM_LDFLAGS += -arch arm64
else
AOM_LDFLAGS += -arch $(ARCH)
endif
endif
ifdef HAVE_ANDROID
# vpx configure.sh overrides our sysroot and it looks for it itself, and
# uses that path to look for the compiler (which we already know)
AOM_CONF += --sdk-path=$(shell dirname $(shell which $(HOST)-gcc))
# put sysroot
AOM_CONF += --libc=$(ANDROID_NDK)/platforms/$(ANDROID_API)/arch-$(PLATFORM_SHORT_ARCH)
endif

ifndef WITH_OPTIMIZATION
AOM_CONF += --enable-debug --disable-optimizations
endif

.aom: aom
	cd $< && LDFLAGS="$(AOM_LDFLAGS)" CROSS=$(AOM_CROSS) ./configure --target=$(AOM_TARGET) \
		$(AOM_CONF) --prefix=$(PREFIX)
	cd $< && $(MAKE)
	cd $< && ../../../contrib/src/pkg-static.sh aom.pc
	cd $< && $(MAKE) install
	touch $@
