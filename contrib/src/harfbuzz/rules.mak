# HARFBUZZ

HARFBUZZ_VERSION := 2.6.4
HARFBUZZ_URL := http://www.freedesktop.org/software/harfbuzz/release/harfbuzz-$(HARFBUZZ_VERSION).tar.xz
PKGS += harfbuzz
ifeq ($(call need_pkg,"harfbuzz"),)
PKGS_FOUND += harfbuzz
endif

$(TARBALLS)/harfbuzz-$(HARFBUZZ_VERSION).tar.xz:
	$(call download_pkg,$(HARFBUZZ_URL),harfbuzz)

.sum-harfbuzz: harfbuzz-$(HARFBUZZ_VERSION).tar.xz

harfbuzz: harfbuzz-$(HARFBUZZ_VERSION).tar.xz .sum-harfbuzz
	$(UNPACK)
	$(APPLY) $(SRC)/harfbuzz/0001-fix-OSAtomic-calls-for-AArch64.patch
	$(APPLY) $(SRC)/harfbuzz/0002-Update-the-bundled-ax_pthread.m4.patch
	$(APPLY) $(SRC)/harfbuzz/0003-Fix-winstore-app-detection-with-mingw64.patch
	$(MOVE)

DEPS_harfbuzz = freetype2 $(DEPS_freetype2)

HARFBUZZ_CONF := --with-freetype \
	--without-glib

ifdef HAVE_DARWIN_OS
HARFBUZZ_CONF += --with-coretext
endif

.harfbuzz: harfbuzz
	$(RECONF)
	cd $< && $(HOSTVARS_PIC) ./configure $(HOSTCONF) $(HARFBUZZ_CONF) ICU_CONFIG=false
	cd $< && $(MAKE) install
	touch $@
