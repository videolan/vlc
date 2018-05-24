# HARFBUZZ

HARFBUZZ_VERSION := 1.7.6
HARFBUZZ_URL := http://www.freedesktop.org/software/harfbuzz/release/harfbuzz-$(HARFBUZZ_VERSION).tar.bz2
PKGS += harfbuzz
ifeq ($(call need_pkg,"harfbuzz"),)
PKGS_FOUND += harfbuzz
endif

$(TARBALLS)/harfbuzz-$(HARFBUZZ_VERSION).tar.bz2:
	$(call download_pkg,$(HARFBUZZ_URL),harfbuzz)

.sum-harfbuzz: harfbuzz-$(HARFBUZZ_VERSION).tar.bz2

harfbuzz: harfbuzz-$(HARFBUZZ_VERSION).tar.bz2 .sum-harfbuzz
	$(UNPACK)
	$(APPLY) $(SRC)/harfbuzz/harfbuzz-aarch64.patch
	$(APPLY) $(SRC)/harfbuzz/harfbuzz-clang.patch
	$(APPLY) $(SRC)/harfbuzz/harfbuzz-fix-freetype-detect.patch
	$(APPLY) $(SRC)/harfbuzz/harfbuzz-fix-coretext-detection.patch
	$(APPLY) $(SRC)/harfbuzz/harfbuzz-create-pkgconfig-file.patch
	$(APPLY) $(SRC)/harfbuzz/harfbuzz-no-tests.patch
	$(MOVE)

DEPS_harfbuzz = freetype2 $(DEPS_freetype2)

.harfbuzz: harfbuzz toolchain.cmake
	cd $< && mkdir -p build && cd build && $(HOSTVARS_PIC) $(CMAKE) \
		-DBUILD_SHARED_LIBS:BOOL=OFF -DHB_HAVE_FREETYPE:BOOL=ON \
		.. && $(MAKE)
	cd $< && cd build && $(MAKE) install
	touch $@
