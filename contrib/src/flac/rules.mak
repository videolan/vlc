# FLAC

FLAC_VERSION := 1.2.1
FLAC_URL := $(SF)/flac/flac-$(FLAC_VERSION).tar.gz

PKGS += flac

$(TARBALLS)/flac-$(FLAC_VERSION).tar.gz:
	$(DOWNLOAD) $(FLAC_URL)

.sum-flac: flac-$(FLAC_VERSION).tar.gz

flac: flac-$(FLAC_VERSION).tar.gz .sum-flac
	$(UNPACK)
	(cd $@-$(FLAC_VERSION) && patch -p1) < $(SRC)/flac/flac-win32.patch
	(cd $@-$(FLAC_VERSION) && patch -p1) < $(SRC)/flac/libFLAC-pc.patch
ifdef HAVE_MACOSX
	cd $<-$(FLAC_VERSION) && sed -e 's,-dynamiclib,-dynamiclib -arch $(ARCH),' -i.orig configure
endif
	mv $@-$(FLAC_VERSION) $@
	touch $@

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

.flac: flac .ogg
	cd $< && $(HOSTVARS) ./configure $(FLACCONF)
	cd $</src && $(MAKE) -C libFLAC install
	cd $< && $(MAKE) -C include install
	touch $@
