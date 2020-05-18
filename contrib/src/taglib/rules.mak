# TagLib

TAGLIB_VERSION := 1.11.1
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
	$(APPLY) $(SRC)/taglib/0001-use-SetFilePointerEx-instead-of-SetFilePointer.patch
	$(APPLY) $(SRC)/taglib/0002-use-GetFileInformationByHandleEx-on-newer-builds-of-.patch
	$(APPLY) $(SRC)/taglib/0003-don-t-use-CreateFile-in-UWP-builds.patch
	$(APPLY) $(SRC)/taglib/use_resolvers_on_streams.patch
	$(MOVE)

.taglib: taglib toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) .
	cd $< && $(MAKE) install
	touch $@
