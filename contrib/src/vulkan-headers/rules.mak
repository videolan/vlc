VULKAN_HEADERS_VERSION := 1.3.219
VULKAN_HEADERS_URL := https://github.com/KhronosGroup/Vulkan-Headers/archive/v$(VULKAN_HEADERS_VERSION).tar.gz

DEPS_vulkan-headers =

$(TARBALLS)/Vulkan-Headers-$(VULKAN_HEADERS_VERSION).tar.gz:
	$(call download_pkg,$(VULKAN_HEADERS_URL),vulkan-headers)

.sum-vulkan-headers: Vulkan-Headers-$(VULKAN_HEADERS_VERSION).tar.gz

vulkan-headers: Vulkan-Headers-$(VULKAN_HEADERS_VERSION).tar.gz .sum-vulkan-headers
	$(UNPACK)
	$(MOVE)

.vulkan-headers: vulkan-headers toolchain.cmake
	rm -f $</build/CMakeCache.txt
	$(HOSTVARS) $(CMAKE) -S $<
	+$(CMAKEBUILD) $</build --target install
	touch $@
