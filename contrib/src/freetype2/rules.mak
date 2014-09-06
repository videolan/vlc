# freetype2

FREETYPE2_VERSION := 2.5.3
FREETYPE2_URL := $(SF)/freetype/freetype2/$(FREETYPE2_VERSION)/freetype-$(FREETYPE2_VERSION).tar.gz

PKGS += freetype2
ifeq ($(call need_pkg,"freetype2"),)
PKGS_FOUND += freetype2
endif

$(TARBALLS)/freetype-$(FREETYPE2_VERSION).tar.gz:
	$(call download,$(FREETYPE2_URL))

.sum-freetype2: freetype-$(FREETYPE2_VERSION).tar.gz

freetype: freetype-$(FREETYPE2_VERSION).tar.gz .sum-freetype2
	$(UNPACK)
	$(call pkg_static, "builds/unix/freetype2.in")
	$(MOVE)

DEPS_freetype = zlib $(DEPS_zlib)

.freetype2: freetype
	sed -i.orig s/-ansi// $</builds/unix/configure
	cd $< && GNUMAKE=$(MAKE) $(HOSTVARS) ./configure --with-harfbuzz=no --with-zlib=yes --without-png $(HOSTCONF)
	cd $< && $(MAKE) && $(MAKE) install
	touch $@
