# sam3.cpp

SAM3_VERSION := 1.0.0
SAM3_URL := $(GITHUB)/PABannier/sam3.cpp/archive/refs/tags/v$(SAM3_VERSION).tar.gz

ifndef HAVE_IOS    # ggml missing clock_gettime() when targeting iOS 9.0
ifndef HAVE_TVOS   # ggml missing std::filesystem::path when targeting tvOS 11.0
ifndef HAVE_MACOSX # ggml missing std::filesystem::path when targeting tvOS 10.13
PKGS += sam3
endif
endif
endif
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
	$(APPLY) $(SRC)/sam3/0001-disable-sam3_decode_video_frame.patch
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
