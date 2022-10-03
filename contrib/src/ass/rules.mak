# ASS
ASS_VERSION := 0.16.0
ASS_URL := $(GITHUB)/libass/libass/releases/download/$(ASS_VERSION)/libass-$(ASS_VERSION).tar.gz

PKGS += ass
ifeq ($(call need_pkg,"libass"),)
PKGS_FOUND += ass
endif

ifdef HAVE_ANDROID
WITH_FONTCONFIG = 0
WITH_HARFBUZZ = 1
ifeq ($(ANDROID_ABI), x86)
WITH_ASS_ASM = 0
endif
else
ifdef HAVE_DARWIN_OS
WITH_FONTCONFIG = 0
WITH_HARFBUZZ = 1
else
ifdef HAVE_WIN32
WITH_FONTCONFIG = 0
WITH_HARFBUZZ = 1
WITH_DWRITE = 1
else
ifdef HAVE_EMSCRIPTEN
WITH_FONTCONFIG = 0
WITH_HARFBUZZ = 1
else
WITH_FONTCONFIG = 1
WITH_HARFBUZZ = 1
endif
endif
endif
endif

$(TARBALLS)/libass-$(ASS_VERSION).tar.gz:
	$(call download_pkg,$(ASS_URL),ass)

.sum-ass: libass-$(ASS_VERSION).tar.gz

libass: libass-$(ASS_VERSION).tar.gz .sum-ass
	$(UNPACK)
	$(call pkg_static,"libass.pc.in")
	$(MOVE)

DEPS_ass = freetype2 $(DEPS_freetype2) fribidi $(DEPS_fribidi) iconv $(DEPS_iconv)

ASS_CONF = --disable-test
ifneq ($(WITH_FONTCONFIG), 0)
DEPS_ass += fontconfig $(DEPS_fontconfig)
else
ASS_CONF += --disable-fontconfig --disable-require-system-font-provider
endif

ifeq ($(WITH_DWRITE), 1)
ASS_CONF += --enable-directwrite
endif

ifneq ($(WITH_HARFBUZZ), 0)
DEPS_ass += harfbuzz $(DEPS_harfbuzz)
else
ASS_CONF += --disable-harfbuzz
endif

ifeq ($(WITH_ASS_ASM), 0)
ASS_CONF += --disable-asm
endif

.ass: libass
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(ASS_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
