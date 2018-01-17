# freetype2

FREETYPE2_VERSION := 2.8.1
FREETYPE2_URL := $(SF)/freetype/freetype2/$(FREETYPE2_VERSION)/freetype-$(FREETYPE2_VERSION).tar.gz

PKGS += freetype2
ifeq ($(call need_pkg,"freetype2"),)
PKGS_FOUND += freetype2
endif

$(TARBALLS)/freetype-$(FREETYPE2_VERSION).tar.gz:
	$(call download_pkg,$(FREETYPE2_URL),freetype2)

.sum-freetype2: freetype-$(FREETYPE2_VERSION).tar.gz

freetype: freetype-$(FREETYPE2_VERSION).tar.gz .sum-freetype2
	$(UNPACK)
	$(call pkg_static, "builds/unix/freetype2.in")
	$(MOVE)

DEPS_freetype2 = zlib $(DEPS_zlib)

.freetype2: freetype
ifndef AD_CLAUSES
	$(REQUIRE_GPL)
endif
	cd $< && cp builds/unix/install-sh .
	sed -i.orig s/-ansi// $</builds/unix/configure
	cd $< && GNUMAKE=$(MAKE) $(HOSTVARS) ./configure --with-harfbuzz=no --with-zlib=yes --without-png --with-bzip2=no $(HOSTCONF)
	cd $< && $(MAKE) && $(MAKE) install
	touch $@
