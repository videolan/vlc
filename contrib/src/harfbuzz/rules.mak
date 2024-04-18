# HARFBUZZ

HARFBUZZ_VERSION := 3.4.0
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
	$(APPLY) $(SRC)/harfbuzz/0001-meson-Enable-big-objects-support-when-building-for-w.patch
	$(APPLY) $(SRC)/harfbuzz/0001-freetype-Fix-function-signatures-to-match-without-ca.patch
	$(APPLY) $(SRC)/harfbuzz/0002-Disable-Wcast-function-type-strict.patch
	$(MOVE)

DEPS_harfbuzz = freetype2 $(DEPS_freetype2)

HARFBUZZ_CONF := -Dfreetype=enabled \
	-Dglib=disabled \
	-Dgobject=disabled \
	-Ddocs=disabled \
	-Dtests=disabled \
	-Dragel_subproject=true

ifdef HAVE_DARWIN_OS
HARFBUZZ_CONF += -Dcoretext=enabled
endif

.harfbuzz: harfbuzz crossfile.meson
	$(MESONCLEAN)
	$(HOSTVARS_MESON) $(MESON) $(HARFBUZZ_CONF)
	+$(MESONBUILD)
	touch $@
