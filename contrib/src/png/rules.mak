# PNG
PNG_VERSION := 1.6.38
PNG_URL := $(SF)/libpng/libpng16/$(PNG_VERSION)/libpng-$(PNG_VERSION).tar.xz

PKGS += png
ifeq ($(call need_pkg,"libpng >= 1.5.4"),)
PKGS_FOUND += png
endif

$(TARBALLS)/libpng-$(PNG_VERSION).tar.xz:
	$(call download_pkg,$(PNG_URL),png)

.sum-png: libpng-$(PNG_VERSION).tar.xz

png: libpng-$(PNG_VERSION).tar.xz .sum-png
	$(UNPACK)
	sed -i.orig 's,set(CMAKE_DEBUG_POSTFIX ,#set(CMAKE_DEBUG_POSTFIX ,' "$(UNPACK_DIR)/CMakeLists.txt"
	$(call pkg_static,"libpng.pc.in")
	$(MOVE)

DEPS_png = zlib $(DEPS_zlib)

PNG_CONF := -DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_EXECUTABLES=OFF

ifdef HAVE_CLANG
ifneq ($(filter arm aarch64, $(ARCH)),)
# TODO this might be set globally and for all targets where intrinsincs are used
PNG_CONF += -DCMAKE_ASM_FLAGS="$(CFLAGS)"
endif
endif

ifdef HAVE_IOS
ifeq ($(ARCH),arm)
# otherwise detection fails
PNG_CONF += -DPNG_ARM_NEON=on
endif
endif

.png: png toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_PIC) $(CMAKE) $(PNG_CONF)
	+$(CMAKEBUILD)
	+$(CMAKEBUILD) --target install
	touch $@
