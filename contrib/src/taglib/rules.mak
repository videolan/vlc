# TagLib

TAGLIB_VERSION := 2.0.1
TAGLIB_URL := $(GITHUB)/taglib/taglib/releases/download/v$(TAGLIB_VERSION)/taglib-$(TAGLIB_VERSION).tar.gz

UTFCPP_VERSION := 3.2.5
UTFCPP_URL := $(GITHUB)/nemtrif/utfcpp/archive/refs/tags/v$(UTFCPP_VERSION).tar.gz

PKGS += taglib
ifeq ($(call need_pkg,"taglib >= 1.9"),)
PKGS_FOUND += taglib
endif

$(TARBALLS)/taglib-$(TAGLIB_VERSION).tar.gz:
	$(call download_pkg,$(TAGLIB_URL),taglib)

$(TARBALLS)/utfcpp-$(UTFCPP_VERSION).tar.gz:
	$(call download_pkg,$(UTFCPP_URL),utfcpp)

.sum-taglib: taglib-$(TAGLIB_VERSION).tar.gz utfcpp-$(UTFCPP_VERSION).tar.gz

.sum-utfcpp: .sum-taglib
	touch $@

taglib: taglib-$(TAGLIB_VERSION).tar.gz .sum-taglib
	$(UNPACK)
	$(MOVE)

taglib/3rdparty/utfcpp: utfcpp-$(UTFCPP_VERSION).tar.gz .sum-utfcpp taglib
	$(UNPACK)
	$(MOVE)

TAGLIB_CONF := -DBUILD_BINDINGS=OFF
ifdef HAVE_WINSTORE
TAGLIB_CONF += -DPLATFORM_WINRT=ON
endif


.taglib: taglib taglib/3rdparty/utfcpp toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(TAGLIB_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
