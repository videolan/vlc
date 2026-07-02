# ASS
ASS_VERSION := 0.17.5
ASS_URL := $(GITHUB)/libass/libass/releases/download/$(ASS_VERSION)/libass-$(ASS_VERSION).tar.xz

PKGS += ass
ifeq ($(call need_pkg,"libass"),)
PKGS_FOUND += ass
endif

ifneq ($(filter aarch64 i386 x86_64, $(ARCH)),)
WITH_ASS_ASM = 1
endif

$(TARBALLS)/libass-$(ASS_VERSION).tar.xz:
	$(call download_pkg,$(ASS_URL),ass)

.sum-ass: libass-$(ASS_VERSION).tar.xz

libass: libass-$(ASS_VERSION).tar.xz .sum-ass
	$(UNPACK)
	$(MOVE)

DEPS_ass = freetype2 $(DEPS_freetype2) fribidi $(DEPS_fribidi) iconv $(DEPS_iconv) harfbuzz $(DEPS_harfbuzz)

ASS_CONF = -Dauto_features=disabled
ifneq ($(BUILD_WITH_FONTCONFIG), 0)
DEPS_ass += fontconfig $(DEPS_fontconfig)
ASS_CONF += -Dfontconfig=enabled
else
ASS_CONF += -Drequire-system-font-provider=false
endif

ifdef HAVE_WIN32
ASS_CONF += -Ddirectwrite=enabled
endif

ifdef HAVE_DARWIN_OS
ASS_CONF += -Dcoretext=enabled
endif

ifeq ($(WITH_ASS_ASM), 1)
ASS_CONF += -Dasm=enabled
endif

.ass: libass crossfile.meson
	$(MESONCLEAN)
	$(MESON) $(ASS_CONF)
	+$(MESONBUILD)
	touch $@
