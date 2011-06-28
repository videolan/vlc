# ebml

EBML_VERSION := 1.2.1
EBML_URL := http://dl.matroska.org/downloads/libebml/libebml-$(EBML_VERSION).tar.bz2
#EBML_URL := $(CONTRIB_VIDEOLAN)/libebml-$(EBML_VERSION).tar.bz2

$(TARBALLS)/libebml-$(EBML_VERSION).tar.bz2:
	$(call download,$(EBML_URL))

.sum-ebml: libebml-$(EBML_VERSION).tar.bz2

libebml: libebml-$(EBML_VERSION).tar.bz2 .sum-ebml
	$(UNPACK)
	mv $@-$(EBML_VERSION) $@
	touch $@

.ebml: libebml
ifdef HAVE_WIN32
	cd $< && $(MAKE) -C make/mingw32 prefix="$(PREFIX)" $(HOSTVARS) SHARED=no
else
	cd $< && $(MAKE) -C make/linux prefix="$(PREFIX)" $(HOSTVARS) staticlib
endif
	cd $< && $(MAKE) -C make/linux install_staticlib install_headers prefix="$(PREFIX)" $(HOSTVARS)
	$(RANLIB) "$(PREFIX)/lib/libebml.a"
	touch $@
