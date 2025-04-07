VULKAN_LOADER_VERSION := 1.3.275
VULKAN_LOADER_URL := $(GITHUB)/KhronosGroup/Vulkan-Loader/archive/v$(VULKAN_LOADER_VERSION).tar.gz

DEPS_vulkan-loader = vulkan-headers $(DEPS_vulkan-headers)

# On WIN32 platform, we don't know where to find the loader
# so always build it for the Vulkan module.
ifdef HAVE_WIN32_DESKTOP
PKGS += vulkan-loader
endif

ifeq ($(call need_pkg,"vulkan"),)
PKGS_FOUND += vulkan-loader
endif

# On Android, vulkan-loader is available on the system itself.
ifdef HAVE_ANDROID
PKGS_FOUND += vulkan-loader
endif

ifndef HAVE_ANDROID
ifdef HAVE_LINUX
DEPS_vulkan-loader += xcb $(DEPS_xcb)
endif
endif

VULKAN_LOADER_CONF := \
	-DBUILD_SHARED_LIBS=OFF \
	-DENABLE_WERROR=OFF \
	-DBUILD_TESTS=OFF

ifndef HAVE_VISUALSTUDIO
# can only use masm or jwasm on Windows
VULKAN_LOADER_CONF += -DUSE_MASM=OFF
endif

$(TARBALLS)/Vulkan-Loader-$(VULKAN_LOADER_VERSION).tar.gz:
	$(call download_pkg,$(VULKAN_LOADER_URL),vulkan-loader)

.sum-vulkan-loader: Vulkan-Loader-$(VULKAN_LOADER_VERSION).tar.gz

vulkan-loader: Vulkan-Loader-$(VULKAN_LOADER_VERSION).tar.gz .sum-vulkan-loader
	$(UNPACK)
# Patches are from msys2 package system
# https://github.com/msys2/MINGW-packages/tree/master/mingw-w64-vulkan-loader
	$(APPLY) $(SRC)/vulkan-loader/0003-fix-libunwind-usage-when-static-linking.patch
ifeq ($(HOST),i686-w64-mingw32)
	cp -v $(SRC)/vulkan-loader/libvulkan-32.def $(UNPACK_DIR)/loader/vulkan-1.def
endif
	$(call pkg_static,"loader/vulkan.pc.in")
	$(MOVE)

# Needed for the loader's cmake script to find the registry files
VULKAN_LOADER_CONF += \
	-DVULKAN_HEADERS_INSTALL_DIR:STRING=$(PREFIX)

.vulkan-loader: vulkan-loader toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(VULKAN_LOADER_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
