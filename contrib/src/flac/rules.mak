# FLAC

FLAC_VERSION := 1.2.1
FLAC_URL := $(SF)/flac/flac-$(FLAC_VERSION).tar.gz

PKGS += flac
ifeq ($(call need_pkg,"flac"),)
PKGS_FOUND += flac
endif

$(TARBALLS)/flac-$(FLAC_VERSION).tar.gz:
	$(call download,$(FLAC_URL))

.sum-flac: flac-$(FLAC_VERSION).tar.gz

flac: flac-$(FLAC_VERSION).tar.gz .sum-flac
	$(UNPACK)
	$(APPLY) $(SRC)/flac/flac-win32.patch
	$(APPLY) $(SRC)/flac/libFLAC-pc.patch
ifdef HAVE_MACOSX
	cd $(UNPACK_DIR) && sed -e 's,-dynamiclib,-dynamiclib -arch $(ARCH),' -i.orig configure
endif
	$(MOVE)

FLACCONF := $(HOSTCONF) \
	--disable--thorough-tests \
	--disable-doxygen-docs \
	--disable-xmms-plugin \
	--disable-cpplibs \
	--disable-oggtest
# TODO? --enable-sse
ifdef HAVE_MACOSX
ifneq ($(findstring $(ARCH),i386 x86_64),)
FLAC_DISABLE_FLAGS += --disable-asm-optimizations
endif
endif

DEPS_flac = ogg $(DEPS_ogg)

.flac: flac
	cd $< && $(HOSTVARS) ./configure $(FLACCONF)
	cd $</src && $(MAKE) -C libFLAC install
	cd $< && $(MAKE) -C include install
	touch $@
