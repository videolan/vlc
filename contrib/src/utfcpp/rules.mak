# utfcpp

UTFCPP_VERSION := 3.2.5
UTFCPP_URL := $(GITHUB)/nemtrif/utfcpp/archive/refs/tags/v$(UTFCPP_VERSION).tar.gz

ifeq ($(call need_pkg,"utfcpp"),)
PKGS_FOUND += utfcpp
endif

$(TARBALLS)/utfcpp-$(UTFCPP_VERSION).tar.gz:
	$(call download_pkg,$(UTFCPP_URL),utfcpp)

.sum-utfcpp: utfcpp-$(UTFCPP_VERSION).tar.gz

utfcpp: utfcpp-$(UTFCPP_VERSION).tar.gz .sum-utfcpp
	$(UNPACK)
	$(MOVE)

UTFCPP_CONF := -DUTF8_TESTS=OFF -DUTF8_SAMPLES=OFF

.utfcpp: utfcpp toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(UTFCPP_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
