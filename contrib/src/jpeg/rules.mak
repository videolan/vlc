# jpeg

JPEG_VERSION := 8c
JPEG_URL := http://www.ijg.org/files/jpegsrc.v$(JPEG_VERSION).tar.gz

PKGS += jpeg

$(TARBALLS)/jpegsrc.v$(JPEG_VERSION).tar.gz:
	$(call download,$(JPEG_URL))

.sum-jpeg: jpegsrc.v$(JPEG_VERSION).tar.gz

jpeg: jpegsrc.v$(JPEG_VERSION).tar.gz .sum-jpeg
	$(UNPACK)
	mv jpeg-$(JPEG_VERSION) jpegsrc.v$(JPEG_VERSION)
	$(MOVE)

.jpeg: jpeg
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	cd $< && $(RANLIB) $(PREFIX)/lib/libjpeg.a
	touch $@
