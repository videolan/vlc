# HARFBUZZ

HARFBUZZ_VERSION := 14.2.1
HARFBUZZ_URL := $(GITHUB)/harfbuzz/harfbuzz/releases/download/$(HARFBUZZ_VERSION)/harfbuzz-$(HARFBUZZ_VERSION).tar.xz
PKGS += harfbuzz
ifeq ($(call need_pkg,"harfbuzz"),)
PKGS_FOUND += harfbuzz
endif

$(TARBALLS)/harfbuzz-$(HARFBUZZ_VERSION).tar.xz:
	$(call download_pkg,$(HARFBUZZ_URL),harfbuzz)

.sum-harfbuzz: harfbuzz-$(HARFBUZZ_VERSION).tar.xz

harfbuzz: harfbuzz-$(HARFBUZZ_VERSION).tar.xz .sum-harfbuzz
	$(UNPACK)
	$(APPLY) $(SRC)/harfbuzz/0001-disable-building-hb-gpu-encode-all-app.patch
	$(MOVE)

DEPS_harfbuzz = freetype2 $(DEPS_freetype2) png $(DEPS_png)

HARFBUZZ_CONF := -Dfreetype=enabled \
	-Dglib=disabled \
	-Dgobject=disabled \
	-Ddocs=disabled \
	-Dtests=disabled \
	-Dgpu_demo=disabled

ifdef HAVE_DARWIN_OS
HARFBUZZ_CONF += -Dcoretext=enabled
endif

.harfbuzz: harfbuzz crossfile.meson
	$(MESONCLEAN)
	$(MESON) $(HARFBUZZ_CONF)
	+$(MESONBUILD)
	touch $@
