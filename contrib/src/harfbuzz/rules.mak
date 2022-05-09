# HARFBUZZ

HARFBUZZ_VERSION := 3.2.0
HARFBUZZ_URL := https://github.com/harfbuzz/harfbuzz/releases/download/$(HARFBUZZ_VERSION)/harfbuzz-$(HARFBUZZ_VERSION).tar.xz
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
	$(MOVE)

DEPS_harfbuzz = freetype2 $(DEPS_freetype2)

HARFBUZZ_CONF := -Dfreetype=enabled \
	-Dglib=disabled \
	-Dgobject=disabled \
	-Ddocs=disabled \
	-Dtests=disabled

ifdef HAVE_DARWIN_OS
HARFBUZZ_CONF += -Dcoretext=enabled
endif

.harfbuzz: harfbuzz crossfile.meson
	cd $< && rm -rf ./build
	cd $< && $(HOSTVARS_MESON) $(MESON) $(HARFBUZZ_CONF) build
	cd $< && cd build && ninja install
	touch $@
