# libplacebo

PLACEBO_VERSION := 1.7.0-rc1
PLACEBO_URL := https://github.com/haasn/libplacebo/archive/v$(PLACEBO_VERSION).tar.gz
PLACEBO_ARCHIVE = libplacebo-$(PLACEBO_VERSION).tar.gz

DEPS_libplacebo = glslang

PKGS += libplacebo
ifeq ($(call need_pkg,"libplacebo"),)
PKGS_FOUND += libplacebo
endif

ifdef HAVE_WIN32
PLACEBOCONF :=
else
PLACEBOCONF := -Dvulkan=enabled \
	-Dglslang=enabled \
	-Dshaderc=disabled
endif

$(TARBALLS)/$(PLACEBO_ARCHIVE):
	$(call download_pkg,$(PLACEBO_URL),libplacebo)

.sum-libplacebo: $(PLACEBO_ARCHIVE)

libplacebo: $(PLACEBO_ARCHIVE) .sum-libplacebo
	$(UNPACK)
	$(APPLY) $(SRC)/libplacebo/0001-meson-fix-glslang-search-path.patch
	$(MOVE)

.libplacebo: libplacebo crossfile.meson
	cd $< && rm -rf ./build
	cd $< && $(HOSTVARS_MESON) $(MESON) $(PLACEBOCONF) build
	cd $< && cd build && ninja install
# Work-around messon issue https://github.com/mesonbuild/meson/issues/4091
	sed -i $(PREFIX)/lib/pkgconfig/libplacebo.pc -e 's/Libs: \(.*\) -L$${libdir} -lplacebo/Libs: -L$${libdir} -lplacebo \1/g'
	touch $@
