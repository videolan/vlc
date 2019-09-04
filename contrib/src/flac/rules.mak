# FLAC

FLAC_VERSION := 1.3.3
FLAC_URL := http://downloads.xiph.org/releases/flac/flac-$(FLAC_VERSION).tar.xz

PKGS += flac
ifeq ($(call need_pkg,"flac"),)
PKGS_FOUND += flac
endif

$(TARBALLS)/flac-$(FLAC_VERSION).tar.xz:
	$(call download_pkg,$(FLAC_URL),flac)

.sum-flac: flac-$(FLAC_VERSION).tar.xz

flac: flac-$(FLAC_VERSION).tar.xz .sum-flac
	$(UNPACK)
	$(APPLY) $(SRC)/flac/mingw-min-max.patch
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/flac/console_write.patch
	$(APPLY) $(SRC)/flac/remove_blocking_code_useless_flaclib.patch
	$(APPLY) $(SRC)/flac/no-createfilea.patch
endif
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
	$(APPLY) $(SRC)/flac/dont-force-msvcrt-version.patch
	$(call pkg_static,"src/libFLAC/flac.pc.in")
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

FLAC_CFLAGS := $(CFLAGS)
ifdef HAVE_WIN32
FLAC_CFLAGS += -mstackrealign
FLAC_CFLAGS +="-DFLAC__NO_DLL"
endif

DEPS_flac = ogg $(DEPS_ogg)

.flac: flac
	cd $< && $(AUTORECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(FLAC_CFLAGS)" ./configure $(FLACCONF)
	cd $< && $(MAKE) -C include install
	cd $< && $(MAKE) -C src/libFLAC install && $(MAKE) -C src/share install
	touch $@
