# TagLib

TAGLIB_VERSION := 2.0
TAGLIB_URL := https://taglib.org/releases/taglib-$(TAGLIB_VERSION).tar.gz
UTFCPP_GITURL := $(GITHUB)/nemtrif/utfcpp.git
UTFCPP_GITVERSION := df857efc5bbc2aa84012d865f7d7e9cccdc08562

PKGS += taglib
ifeq ($(call need_pkg,"taglib >= 1.9"),)
PKGS_FOUND += taglib
endif

$(TARBALLS)/taglib-$(TAGLIB_VERSION).tar.gz:
	$(call download_pkg,$(TAGLIB_URL),taglib)

.sum-taglib: taglib-$(TAGLIB_VERSION).tar.gz

taglib: taglib-$(TAGLIB_VERSION).tar.gz .sum-taglib
	$(UNPACK)
	cd $(UNPACK_DIR)/3rdparty && git clone -n $(UTFCPP_GITURL) utfcpp
	cd $(UNPACK_DIR)/3rdparty/utfcpp && git checkout $(UTFCPP_GITVERSION)
	$(MOVE)

TAGLIB_CONF := -DBUILD_BINDINGS=OFF
ifdef HAVE_WINSTORE
TAGLIB_CONF += -DPLATFORM_WINRT=ON
endif


.taglib: taglib toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(TAGLIB_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
