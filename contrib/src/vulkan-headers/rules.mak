VULKAN_HEADERS_VERSION := 1.4.355
VULKAN_HEADERS_URL := $(GITHUB)/KhronosGroup/Vulkan-Headers/archive/v$(VULKAN_HEADERS_VERSION).tar.gz

DEPS_vulkan-headers =

VULKAN_HEADERS_MIN_VERSION := 1.3.219

VULKAN_HEADERS_MIN_VERSION_LIST := $(subst ., ,$(VULKAN_HEADERS_MIN_VERSION))
VULKAN_HEADERS_MIN_VERSION_NAME := $(subst $() ,_,$(wordlist 1,2,$(VULKAN_HEADERS_MIN_VERSION_LIST)))

# VK_MAKE_API_VERSION uses (uint32_t) casts which prevent the preprocessor
# from understanding comparisons against a specific patch version. Use the
# VK_VERSION_X_Y header guard defines instead to detect major/minor, and
# then use VK_HEADER_VERSION which is not defined as a cast for the patch
# version.
define VULKAN_HEADERS_CHECK :=
# include <vulkan/vulkan_core.h> \n
# if defined(VK_VERSION_$(VULKAN_HEADERS_MIN_VERSION_NAME)) \n
#    && VK_HEADER_VERSION >= $(lastword $(VULKAN_HEADERS_MIN_VERSION_LIST)) \n
#  define VULKAN_HEADERS_OK \n
# endif \n
endef

PKGS += vulkan-headers
ifndef HAVE_ANDROID # in NDK27 vk.xml is not available anymore, we need to install our own
ifneq ($(call cppcheck, VULKAN_HEADERS_OK, $(VULKAN_HEADERS_CHECK)),)
PKGS_FOUND += vulkan-headers
endif
endif

$(TARBALLS)/Vulkan-Headers-$(VULKAN_HEADERS_VERSION).tar.gz:
	$(call download_pkg,$(VULKAN_HEADERS_URL),vulkan-headers)

.sum-vulkan-headers: Vulkan-Headers-$(VULKAN_HEADERS_VERSION).tar.gz

vulkan-headers: Vulkan-Headers-$(VULKAN_HEADERS_VERSION).tar.gz .sum-vulkan-headers
	$(UNPACK)
	# avoid unnecessary dependency on CMake 3.22
	sed -i.orig 's,VERSION 3.22.1, VERSION 3.21,' $(UNPACK_DIR)/CMakeLists.txt
	$(MOVE)

.vulkan-headers: vulkan-headers toolchain.cmake
	rm -rf $(PREFIX)/include/vulkan
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
