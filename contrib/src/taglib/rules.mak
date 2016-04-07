# TagLib

TAGLIB_VERSION := 1.10beta
TAGLIB_URL := http://taglib.github.io/releases/taglib-$(TAGLIB_VERSION).tar.gz

PKGS += taglib
ifeq ($(call need_pkg,"taglib >= 1.9"),)
PKGS_FOUND += taglib
endif

$(TARBALLS)/taglib-$(TAGLIB_VERSION).tar.gz:
	$(call download,$(TAGLIB_URL))

.sum-taglib: taglib-$(TAGLIB_VERSION).tar.gz

taglib: taglib-$(TAGLIB_VERSION).tar.gz .sum-taglib
	$(UNPACK)
ifdef HAVE_WINRT
	$(APPLY) $(SRC)/taglib/unicode.patch
endif
	$(MOVE)

.taglib: taglib toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) \
		-DENABLE_STATIC:BOOL=ON \
		.
	cd $< && $(MAKE) install
	touch $@
