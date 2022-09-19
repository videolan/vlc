# FLAC

FLAC_VERSION := 1.4.0
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
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/flac/console_write.patch
	$(APPLY) $(SRC)/flac/remove_blocking_code_useless_flaclib.patch
	$(APPLY) $(SRC)/flac/no-createfilew.patch
endif
ifdef HAVE_DARWIN_OS
	cd $(UNPACK_DIR) && sed -e 's,-dynamiclib,-dynamiclib -arch $(ARCH),' -i.orig configure
endif
	$(APPLY) $(SRC)/flac/dont-force-msvcrt-version.patch
	$(call pkg_static,"src/libFLAC/flac.pc.in")
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

FLACCONF := \
	--disable-examples \
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
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) CFLAGS="$(FLAC_CFLAGS)" $(FLACCONF)
	$(MAKEBUILD) -C include install
	$(MAKEBUILD) -C src/libFLAC install
	$(MAKEBUILD) -C src/share install
	touch $@
