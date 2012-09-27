# tiff

TIFF_VERSION := 4.0.3
TIFF_URL := http://download.osgeo.org/libtiff/tiff-$(TIFF_VERSION).tar.gz

$(TARBALLS)/tiff-$(TIFF_VERSION).tar.gz:
	$(call download,$(TIFF_URL))

.sum-tiff: tiff-$(TIFF_VERSION).tar.gz

tiff: tiff-$(TIFF_VERSION).tar.gz .sum-tiff
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.tiff: tiff
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) \
		--disable-jpeg \
		--disable-zlib \
		--disable-cxx \
		--without-x
	cd $< && $(MAKE) -C port && $(MAKE) -C libtiff
	cd $< && $(MAKE) install
	touch $@
