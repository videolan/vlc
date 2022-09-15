# tiff

TIFF_VERSION := 4.0.9
TIFF_URL := http://download.osgeo.org/libtiff/tiff-$(TIFF_VERSION).tar.gz

$(TARBALLS)/tiff-$(TIFF_VERSION).tar.gz:
	$(call download_pkg,$(TIFF_URL),tiff)

.sum-tiff: tiff-$(TIFF_VERSION).tar.gz

tiff: tiff-$(TIFF_VERSION).tar.gz .sum-tiff
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(APPLY) $(SRC)/tiff/tiff-winstore.patch
	$(MOVE)
	mv tiff/config.sub tiff/config.guess tiff/config

.tiff: tiff
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) \
		--disable-jpeg \
		--disable-zlib \
		--disable-cxx \
		--without-x
	$(MAKE) -C $< -C port
	$(MAKE) -C $< -C libtiff
	$(MAKE) -C $< -C libtiff install
	touch $@
