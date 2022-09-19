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
	mkdir -p $</_build
	cd $</_build && $(HOSTVARS) ../configure $(HOSTCONF) \
		--disable-jpeg \
		--disable-zlib \
		--disable-cxx \
		--without-x
	$(MAKE) -C $</_build -C port
	$(MAKE) -C $</_build -C libtiff
	$(MAKE) -C $</_build -C libtiff install
	touch $@
