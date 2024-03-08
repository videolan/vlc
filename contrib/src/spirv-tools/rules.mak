# spirv-tools

SPIRVTOOLS_VERSION := 2023.2
SPIRVTOOLS_URL := $(GITHUB)/KhronosGroup/SPIRV-Tools/archive/refs/tags/v$(SPIRVTOOLS_VERSION).tar.gz

SPIRVHEADERS_VERSION := 1.3.246.1
SPIRVHEADERS_URL := $(GITHUB)/KhronosGroup/SPIRV-Headers/archive/refs/tags/sdk-$(SPIRVHEADERS_VERSION).tar.gz

PKGS_TOOLS += spirv-tools
ifeq ($(call need_pkg,"SPIRV-Tools >= $(SPIRVTOOLS_VERSION).1"),)
PKGS_FOUND += spirv-tools
endif

$(TARBALLS)/SPIRV-Headers-sdk-$(SPIRVHEADERS_VERSION).tar.gz:
	$(call download,$(SPIRVHEADERS_URL))

$(TARBALLS)/SPIRV-Tools-$(SPIRVTOOLS_VERSION).tar.gz:
	$(call download,$(SPIRVTOOLS_URL))

.sum-spirv-tools: SPIRV-Tools-$(SPIRVTOOLS_VERSION).tar.gz SPIRV-Headers-sdk-$(SPIRVHEADERS_VERSION).tar.gz

spirv-tools: SPIRV-Tools-$(SPIRVTOOLS_VERSION).tar.gz .sum-spirv-tools
	$(UNPACK)
	$(MOVE)

spirv-tools/external/spirv-headers: SPIRV-Headers-sdk-$(SPIRVHEADERS_VERSION).tar.gz .sum-spirv-tools spirv-tools
	$(UNPACK)
	$(MOVE)

SPIRVTOOLS_CONFIG := -DSPIRV_SKIP_TESTS=ON

.spirv-tools: BUILD_DIR=$</vlc_native
.spirv-tools: spirv-tools spirv-tools/external/spirv-headers
	$(CMAKECLEAN)
	$(BUILDVARS) $(CMAKE_NATIVE) $(SPIRVTOOLS_CONFIG)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)

	touch $@
