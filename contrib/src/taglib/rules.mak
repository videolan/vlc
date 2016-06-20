# TagLib

TAGLIB_VERSION := 1.11
TAGLIB_URL := http://taglib.github.io/releases/taglib-$(TAGLIB_VERSION).tar.gz

PKGS += taglib
ifeq ($(call need_pkg,"taglib >= 1.9"),)
PKGS_FOUND += taglib
endif

$(TARBALLS)/taglib-$(TAGLIB_VERSION).tar.gz:
	$(call download_pkg,$(TAGLIB_URL),taglib)

.sum-taglib: taglib-$(TAGLIB_VERSION).tar.gz

taglib: taglib-$(TAGLIB_VERSION).tar.gz .sum-taglib
	$(UNPACK)
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/taglib/unicode.patch
endif
	$(MOVE)

.taglib: taglib toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) \
		-DBUILD_SHARED_LIBS:BOOL=OFF \
		.
	cd $< && $(MAKE) install
	touch $@
