# HARFBUZZ

HARFBUZZ_VERSION := 1.7.6
HARFBUZZ_URL := http://www.freedesktop.org/software/harfbuzz/release/harfbuzz-$(HARFBUZZ_VERSION).tar.bz2
PKGS += harfbuzz
ifeq ($(call need_pkg,"harfbuzz"),)
PKGS_FOUND += harfbuzz
endif

ifdef HAVE_DARWIN_OS
HARFBUZZCONF += --with-coretext=yes
endif

$(TARBALLS)/harfbuzz-$(HARFBUZZ_VERSION).tar.bz2:
	$(call download_pkg,$(HARFBUZZ_URL),harfbuzz)

.sum-harfbuzz: harfbuzz-$(HARFBUZZ_VERSION).tar.bz2

harfbuzz: harfbuzz-$(HARFBUZZ_VERSION).tar.bz2 .sum-harfbuzz
	$(UNPACK)
	$(APPLY) $(SRC)/harfbuzz/harfbuzz-aarch64.patch
	$(APPLY) $(SRC)/harfbuzz/harfbuzz-clang.patch
	$(APPLY) $(SRC)/harfbuzz/Cmake-test.patch
	$(APPLY) $(SRC)/harfbuzz/0001-Skip-using-the-_BitScan-intrinsics-on-mingw.patch
	$(MOVE)

DEPS_harfbuzz = freetype2 $(DEPS_freetype2)

.harfbuzz: harfbuzz toolchain.cmake
	cd $< && mkdir -p build && cd build && $(HOSTVARS_PIC) $(CMAKE) \
		-DBUILD_SHARED_LIBS:BOOL=OFF -DHB_HAVE_FREETYPE:BOOL=ON \
		.. && $(MAKE)
	cd $< && cd build && $(MAKE) install
	touch $@
