VULKAN_LOADER_VERSION := 1.1.127
VULKAN_LOADER_URL := https://github.com/KhronosGroup/Vulkan-Loader/archive/v$(VULKAN_LOADER_VERSION).tar.gz

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

ifdef HAVE_LINUX
DEPS_vulkan-loader += xcb $(DEPS_xcb)
endif

VULKAN_LOADER_CONF := \
	-DENABLE_STATIC_LOADER=ON \
	-DBUILD_SHARED_LIBS=OFF \
	-DBUILD_TESTS=OFF \
	-DBUILD_LOADER=ON \
	-DCMAKE_ASM_COMPILER="$(AS)"

$(TARBALLS)/Vulkan-Loader-$(VULKAN_LOADER_VERSION).tar.gz:
	$(call download_pkg,$(VULKAN_LOADER_URL),vulkan-loader)

.sum-vulkan-loader: Vulkan-Loader-$(VULKAN_LOADER_VERSION).tar.gz

vulkan-loader: Vulkan-Loader-$(VULKAN_LOADER_VERSION).tar.gz .sum-vulkan-loader
	$(UNPACK)
# Patches are from msys2 package system
# https://github.com/msys2/MINGW-packages/tree/master/mingw-w64-vulkan-loader
	$(APPLY) $(SRC)/vulkan-loader/001-build-fix.patch
	$(APPLY) $(SRC)/vulkan-loader/002-proper-def-files-for-32bit.patch
	$(APPLY) $(SRC)/vulkan-loader/003-generate-pkgconfig-files.patch
	$(MOVE)

# Needed for the loader's cmake script to find the registry files
VULKAN_LOADER_ENV_CONF = \
	VULKAN_HEADERS_INSTALL_DIR="$(PREFIX)"

.vulkan-loader: vulkan-loader toolchain.cmake
	cd $< && rm -rf ./build && mkdir -p build
	cd $</build && $(VULKAN_LOADER_ENV_CONF) $(HOSTVARS) \
		$(CMAKE) $(VULKAN_LOADER_CONF) ..
	cd $</build && $(CMAKEBUILD) .

ifdef HAVE_WIN32
# CMake will generate a .pc file with -lvulkan even if the static library
# generated is libVKstatic.1.a. It also forget to link with libcfgmgr32.
	cd $< && sed -i.orig -e "s,-lvulkan,-lVKstatic.1 -lcfgmgr32," build/loader/vulkan.pc
endif

	$(call pkg_static,"build/loader/vulkan.pc")
	+$(CMAKEBUILD) $</build --target install
	touch $@
