# jpeg

JPEG_VERSION := 9b
JPEG_URL := http://www.ijg.org/files/jpegsrc.v$(JPEG_VERSION).tar.gz

$(TARBALLS)/jpegsrc.v$(JPEG_VERSION).tar.gz:
	$(call download_pkg,$(JPEG_URL),jpeg)

.sum-jpeg: jpegsrc.v$(JPEG_VERSION).tar.gz

jpeg: jpegsrc.v$(JPEG_VERSION).tar.gz .sum-jpeg
	$(UNPACK)
	mv jpeg-$(JPEG_VERSION) jpegsrc.v$(JPEG_VERSION)
	$(APPLY) $(SRC)/jpeg/no_executables.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.jpeg: jpeg
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	cd $< && if test -e $(PREFIX)/lib/libjpeg.a; then $(RANLIB) $(PREFIX)/lib/libjpeg.a; fi
	touch $@
