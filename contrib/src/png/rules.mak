# PNG
PNG_VERSION := 1.4.7
PNG_URL := $(SF)/libpng/libpng-$(PNG_VERSION).tar.bz2

PKGS += png

$(TARBALLS)/libpng-$(PNG_VERSION).tar.bz2:
	$(call download,$(PNG_URL))

.sum-png: libpng-$(PNG_VERSION).tar.bz2

png: libpng-$(PNG_VERSION).tar.bz2 .sum-png
	$(UNPACK)
	$(MOVE)

.png: png .zlib
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
