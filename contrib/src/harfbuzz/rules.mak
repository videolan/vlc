# HARFBUZZ

HARFBUZZ_VERSION := 0.9.2
HARFBUZZ_URL := http://www.freedesktop.org/software/harfbuzz/snapshot/harfbuzz-$(HARFBUZZ_VERSION).tar.bz2
PKGS += harfbuzz
ifeq ($(call need_pkg,"harfbuzz"),)
PKGS_FOUND += harfbuzz
endif

$(TARBALLS)/harfbuzz-$(HARFBUZZ_VERSION).tar.bz2:
	$(call download,$(HARFBUZZ_URL))

.sum-harfbuzz: harfbuzz-$(HARFBUZZ_VERSION).tar.bz2

harfbuzz: harfbuzz-$(HARFBUZZ_VERSION).tar.bz2 .sum-harfbuzz
	$(UNPACK)
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/harfbuzz/harfbuzz-osx.patch
endif
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

DEPS_harfbuzz = freetype2 $(DEPS_freetype2)

HARFBUZZ_CONF=

.harfbuzz: harfbuzz
ifdef HAVE_MACOSX
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS)"  ./autogen.sh $(HOSTCONF) $(HARFBUZZ_CONF)
else
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS)" ./configure $(HOSTCONF) $(HARFBUZZ_CONF)
endif
	cd $< && $(MAKE) install
	touch $@
