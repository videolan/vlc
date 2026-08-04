# TagLib

TAGLIB_VERSION := 2.3
TAGLIB_URL := $(GITHUB)/taglib/taglib/releases/download/v$(TAGLIB_VERSION)/taglib-$(TAGLIB_VERSION).tar.gz

PKGS += taglib
ifeq ($(call need_pkg,"taglib >= 1.9"),)
PKGS_FOUND += taglib
endif

DEPS_taglib := utfcpp $(DEPS_utfcpp)

$(TARBALLS)/taglib-$(TAGLIB_VERSION).tar.gz:
	$(call download_pkg,$(TAGLIB_URL),taglib)

.sum-taglib: taglib-$(TAGLIB_VERSION).tar.gz

taglib: taglib-$(TAGLIB_VERSION).tar.gz .sum-taglib
	$(UNPACK)
	$(MOVE)

TAGLIB_CONF := -DBUILD_BINDINGS=OFF
ifdef HAVE_WINSTORE
TAGLIB_CONF += -DPLATFORM_WINRT=ON
endif


.taglib: taglib toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(TAGLIB_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
