# freetype2

FREETYPE2_VERSION := 2.4.5
FREETYPE2_URL := $(SF)/freetype/freetype-$(FREETYPE2_VERSION).tar.gz

NEED_FREETYPE2 := $(call need_pkg,"freetype2")

$(TARBALLS)/freetype-$(FREETYPE2_VERSION).tar.gz:
	$(call download,$(FREETYPE2_URL))

.sum-freetype2: freetype-$(FREETYPE2_VERSION).tar.gz

freetype: freetype-$(FREETYPE2_VERSION).tar.gz .sum-freetype2
	$(UNPACK)
	mv $@-$(FREETYPE2_VERSION) $@
	touch $@

ifeq ($(NEED_FREETYPE2),)
.freetype2:
else
PKGS += freetype2

.freetype2: freetype
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
endif
	touch $@
