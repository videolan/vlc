# FLAC

FLAC_VERSION := 1.3.0
FLAC_URL := http://downloads.xiph.org/releases/flac/flac-$(FLAC_VERSION).tar.xz

PKGS += flac
ifeq ($(call need_pkg,"flac"),)
PKGS_FOUND += flac
endif

$(TARBALLS)/flac-$(FLAC_VERSION).tar.xz:
	$(call download,$(FLAC_URL))

.sum-flac: flac-$(FLAC_VERSION).tar.xz

flac: flac-$(FLAC_VERSION).tar.xz .sum-flac
	$(UNPACK)
	$(APPLY) $(SRC)/flac/libFLAC-pc.patch
ifdef HAVE_DARWIN_OS
	cd $(UNPACK_DIR) && sed -e 's,-dynamiclib,-dynamiclib -arch $(ARCH),' -i.orig configure
endif
ifdef HAVE_ANDROID
ifeq ($(ANDROID_ABI), x86)
	# cpu.c:130:29: error: sys/ucontext.h: No such file or directory
	# defining USE_OBSOLETE_SIGCONTEXT_FLAVOR allows us to bypass that
	cd $(UNPACK_DIR) && sed -i.orig -e s/"#  undef USE_OBSOLETE_SIGCONTEXT_FLAVOR"/"#define USE_OBSOLETE_SIGCONTEXT_FLAVOR"/g src/libFLAC/cpu.c
endif
endif
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

FLACCONF := $(HOSTCONF) \
	--disable-thorough-tests \
	--disable-doxygen-docs \
	--disable-xmms-plugin \
	--disable-cpplibs \
	--disable-oggtest
# TODO? --enable-sse
ifdef HAVE_DARWIN_OS
ifneq ($(findstring $(ARCH),i386 x86_64),)
FLACCONF += --disable-asm-optimizations
endif
endif

DEPS_flac = ogg $(DEPS_ogg)

.flac: flac
	cd $< && $(HOSTVARS) ./configure $(FLACCONF)
	cd $</include && $(MAKE) install
	cd $</src && $(MAKE) -C share install && $(MAKE) -C libFLAC install
	touch $@
