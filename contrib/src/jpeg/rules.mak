# jpeg

JPEG_VERSION := 9a
#JPEG_URL := http://www.ijg.org/files/jpegsrc.v$(JPEG_VERSION).tar.gz
JPEG_URL := $(CONTRIB_VIDEOLAN)/jpegsrc.v$(JPEG_VERSION).tar.gz

$(TARBALLS)/jpegsrc.v$(JPEG_VERSION).tar.gz:
	$(call download,$(JPEG_URL))

.sum-jpeg: jpegsrc.v$(JPEG_VERSION).tar.gz

jpeg: jpegsrc.v$(JPEG_VERSION).tar.gz .sum-jpeg
	$(UNPACK)
	mv jpeg-$(JPEG_VERSION) jpegsrc.v$(JPEG_VERSION)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.jpeg: jpeg
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	cd $< && if test -e $(PREFIX)/lib/libjpeg.a; then $(RANLIB) $(PREFIX)/lib/libjpeg.a; fi
	touch $@
