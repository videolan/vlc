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
	$(APPLY) $(SRC)/harfbuzz/harfbuzz-aarch64.patch
	#It uses Cmake builtin to detect freetype instead of Pkg-Config
	$(APPLY) $(SRC)/harfbuzz/harfbuzz-fix-freetype-detect.patch
	#It does not search for CoreText Framework everywhere
	$(APPLY) $(SRC)/harfbuzz/harfbuzz-fix-coretext-detection.patch
	#Missing Frameworks in the pkgconfig file
	$(APPLY) $(SRC)/harfbuzz/harfbuzz-create-pkgconfig-file.patch
	$(APPLY) $(SRC)/harfbuzz/0001-CMakeLists-Enable-big-objects-support-for-win64.patch
	$(MOVE)

DEPS_harfbuzz = freetype2 $(DEPS_freetype2)

.harfbuzz: harfbuzz toolchain.cmake
	cd $< && mkdir -p build && cd build && $(HOSTVARS_PIC) $(CMAKE) \
		-DBUILD_SHARED_LIBS:BOOL=OFF \
		-DHB_HAVE_FREETYPE:BOOL=ON \
		-DHB_BUILD_TESTS=OFF \
		-DHB_BUILD_UTILS=OFF \
		-DHB_HAVE_GLIB=OFF \
		.. && $(MAKE)
	cd $< && cd build && $(MAKE) install
	touch $@
