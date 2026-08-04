# TagLib

TAGLIB_VERSION := 2.3
TAGLIB_URL := $(GITHUB)/taglib/taglib/releases/download/v$(TAGLIB_VERSION)/taglib-$(TAGLIB_VERSION).tar.gz

UTF8CPP_VERSION := 3.2.5
UTF8CPP_URL := $(GITHUB)/nemtrif/utfcpp/archive/refs/tags/v$(UTF8CPP_VERSION).tar.gz

PKGS += taglib
ifeq ($(call need_pkg,"taglib >= 1.9"),)
PKGS_FOUND += taglib
endif

$(TARBALLS)/taglib-$(TAGLIB_VERSION).tar.gz:
	$(call download_pkg,$(TAGLIB_URL),taglib)

$(TARBALLS)/utf8cpp-$(UTF8CPP_VERSION).tar.gz:
	$(call download_pkg,$(UTF8CPP_URL),utfcpp)

.sum-taglib: taglib-$(TAGLIB_VERSION).tar.gz utf8cpp-$(UTF8CPP_VERSION).tar.gz

.sum-utf8cpp: .sum-taglib
	touch $@

taglib: taglib-$(TAGLIB_VERSION).tar.gz .sum-taglib
	$(UNPACK)
	$(MOVE)

taglib/3rdparty/utfcpp: utf8cpp-$(UTF8CPP_VERSION).tar.gz .sum-utf8cpp taglib
	$(UNPACK)
	$(MOVE)

TAGLIB_CONF := -DBUILD_BINDINGS=OFF
ifdef HAVE_WINSTORE
TAGLIB_CONF += -DPLATFORM_WINRT=ON
endif


.taglib: taglib taglib/3rdparty/utf8cpp toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(TAGLIB_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
