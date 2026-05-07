# sam3.cpp

SAM3_VERSION := 1.0.0
SAM3_URL := $(GITHUB)/PABannier/sam3.cpp/archive/refs/tags/v$(SAM3_VERSION).tar.gz

PKGS += sam3
ifeq ($(call need_pkg,"sam3"),)
PKGS_FOUND += sam3
endif

DEPS_sam3 = ggml $(DEPS_ggml)

$(TARBALLS)/sam3.cpp-$(SAM3_VERSION).tar.gz:
	$(call download_pkg,$(SAM3_URL),sam3)

.sum-sam3: sam3.cpp-$(SAM3_VERSION).tar.gz

sam3: sam3.cpp-$(SAM3_VERSION).tar.gz .sum-sam3
	$(UNPACK)
	$(APPLY) $(SRC)/sam3/0001-add-install-rules.patch
	$(MOVE)

SAM3_CONF := \
	-DSAM3_METAL=OFF \
	-DSAM3_BUILD_EXAMPLES=OFF \
	-DSAM3_BUILD_TESTS=OFF

.sam3: sam3 toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(SAM3_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
