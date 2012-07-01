# TagLib

TAGLIB_VERSION := 1.7.2
TAGLIB_URL := https://github.com/downloads/taglib/taglib/taglib-$(TAGLIB_VERSION).tar.gz

PKGS += taglib

$(TARBALLS)/taglib-$(TAGLIB_VERSION).tar.gz:
	$(call download,$(TAGLIB_URL))

.sum-taglib: taglib-$(TAGLIB_VERSION).tar.gz

taglib: taglib-$(TAGLIB_VERSION).tar.gz .sum-taglib
	$(UNPACK)
	$(APPLY) $(SRC)/taglib/taglib-static.patch
	$(APPLY) $(SRC)/taglib/no-ansi.patch
	$(APPLY) $(SRC)/taglib/7036.patch
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/taglib/taglib-win32.patch
endif
	$(MOVE)

.taglib: taglib toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) \
		-DENABLE_STATIC:BOOL=ON \
		-DWITH_ASF:BOOL=ON \
		-DWITH_MP4:BOOL=ON .
	cd $< && $(MAKE) install
	touch $@
