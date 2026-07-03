# jpeg

JPEG_VERSION := 9f
JPEG_URL := http://www.ijg.org/files/jpegsrc.v$(JPEG_VERSION).tar.gz

$(TARBALLS)/jpegsrc.v$(JPEG_VERSION).tar.gz:
	$(call download_pkg,$(JPEG_URL),jpeg)

.sum-jpeg: jpegsrc.v$(JPEG_VERSION).tar.gz

jpeg: UNPACK_DIR=jpeg-$(JPEG_VERSION)
jpeg: jpegsrc.v$(JPEG_VERSION).tar.gz .sum-jpeg
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.jpeg: jpeg
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE)
	+$(MAKEBUILD) bin_PROGRAMS=
	+$(MAKEBUILD) bin_PROGRAMS= install
	if test -e $(PREFIX)/lib/libjpeg.a; then $(RANLIB) $(PREFIX)/lib/libjpeg.a; fi
	touch $@
