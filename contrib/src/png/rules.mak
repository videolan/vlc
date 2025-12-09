# PNG
PNG_VERSION := 1.6.53
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
	$(APPLY) $(SRC)/png/0001-Put-the-build-include-include-before-the-CMake-Platf.patch
	$(call pkg_static,"libpng.pc.in")
	$(MOVE)

DEPS_png = zlib $(DEPS_zlib)

PNG_CONF := -DPNG_SHARED=OFF -DPNG_TESTS=OFF -DPNG_TOOLS=OFF -DPNG_FRAMEWORK=OFF

PNG_CONF += -DPNG_DEBUG_POSTFIX:STRING=

ifeq ($(ARCH),arm)
ifdef HAVE_IOS
# otherwise detection fails
PNG_CONF += -DPNG_ARM_NEON=on
else ifdef HAVE_WIN32
# No runtime detection needed
PNG_CONF += -DPNG_ARM_NEON=on
else ifdef HAVE_ANDROID
# libpng disallows "check" for ARM here as it would be redundant/"unproductive",
# see https://github.com/glennrp/libpng/commit/b8ca9108acddfb9fb5d886f5e8a072ebaf436dbb
PNG_CONF += -DPNG_ARM_NEON=on
else
# Otherwise do runtime detection
PNG_CONF += -DPNG_ARM_NEON=check
endif
endif

.png: png toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(PNG_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
