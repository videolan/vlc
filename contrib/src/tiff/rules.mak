# tiff

TIFF_VERSION := 4.0.7
TIFF_URL := http://download.osgeo.org/libtiff/tiff-$(TIFF_VERSION).tar.gz

$(TARBALLS)/tiff-$(TIFF_VERSION).tar.gz:
	$(call download_pkg,$(TIFF_URL),tiff)

.sum-tiff: tiff-$(TIFF_VERSION).tar.gz

tiff: tiff-$(TIFF_VERSION).tar.gz .sum-tiff
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)
	mv tiff/config.sub tiff/config.guess tiff/config

.tiff: tiff
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) \
		--disable-jpeg \
		--disable-zlib \
		--disable-cxx \
		--without-x
	cd $< && $(MAKE) -C port && $(MAKE) -C libtiff
	cd $< && $(MAKE) install
	touch $@
