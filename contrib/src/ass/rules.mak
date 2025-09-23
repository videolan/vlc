# ASS
ASS_VERSION := 0.17.4
ASS_URL := $(GITHUB)/libass/libass/releases/download/$(ASS_VERSION)/libass-$(ASS_VERSION).tar.gz

PKGS += ass
ifeq ($(call need_pkg,"libass"),)
PKGS_FOUND += ass
endif

ifneq ($(filter aarch64 i386 x86_64, $(ARCH)),)
WITH_ASS_ASM = 1
endif

ifdef HAVE_ANDROID
WITH_FONTCONFIG = 0
else
ifdef HAVE_DARWIN_OS
WITH_FONTCONFIG = 0
else
ifdef HAVE_WIN32
WITH_FONTCONFIG = 0
WITH_DWRITE = 1
else
ifdef HAVE_EMSCRIPTEN
WITH_FONTCONFIG = 0
else
WITH_FONTCONFIG = 1
endif
endif
endif
endif

$(TARBALLS)/libass-$(ASS_VERSION).tar.gz:
	$(call download_pkg,$(ASS_URL),ass)

.sum-ass: libass-$(ASS_VERSION).tar.gz

libass: libass-$(ASS_VERSION).tar.gz .sum-ass
	$(UNPACK)
	$(MOVE)

DEPS_ass = freetype2 $(DEPS_freetype2) fribidi $(DEPS_fribidi) iconv $(DEPS_iconv) harfbuzz $(DEPS_harfbuzz)

ASS_CONF = -Dauto_features=disabled
ifneq ($(WITH_FONTCONFIG), 0)
DEPS_ass += fontconfig $(DEPS_fontconfig)
ASS_CONF += -Dfontconfig=enabled
else
ASS_CONF += -Drequire-system-font-provider=false
endif

ifeq ($(WITH_DWRITE), 1)
ASS_CONF += -Ddirectwrite=enabled
endif

ifeq ($(WITH_ASS_ASM), 1)
ASS_CONF += -Dasm=enabled
endif

.ass: libass crossfile.meson
	$(MESONCLEAN)
	$(MESON) $(ASS_CONF)
	+$(MESONBUILD)
	touch $@
