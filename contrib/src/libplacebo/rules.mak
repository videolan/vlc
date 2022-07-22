# libplacebo

PLACEBO_VERSION := 4.192.1
PLACEBO_ARCHIVE = libplacebo-v$(PLACEBO_VERSION).tar.gz
PLACEBO_URL := https://code.videolan.org/videolan/libplacebo/-/archive/v$(PLACEBO_VERSION)/$(PLACEBO_ARCHIVE)

PLACEBOCONF := -Dglslang=enabled \
	-Dshaderc=disabled \
	-Ddemos=false \
	-Dtests=false

DEPS_libplacebo = glslang

ifndef HAVE_WINSTORE
PKGS += libplacebo
endif
ifeq ($(call need_pkg,"libplacebo >= 2.72"),)
PKGS_FOUND += libplacebo
endif

ifdef HAVE_WIN32
DEPS_libplacebo += pthreads $(DEPS_pthreads)
endif

# We don't want vulkan on darwin for now
ifndef HAVE_DARWIN_OS
DEPS_libplacebo += vulkan-loader $(DEPS_vulkan-loader) vulkan-headers $(DEPS_vulkan-headers)
PLACEBOCONF += -Dvulkan-registry=${PREFIX}/share/vulkan/registry/vk.xml
endif

$(TARBALLS)/$(PLACEBO_ARCHIVE):
	$(call download_pkg,$(PLACEBO_URL),libplacebo)

.sum-libplacebo: $(PLACEBO_ARCHIVE)

libplacebo: $(PLACEBO_ARCHIVE) .sum-libplacebo
	$(UNPACK)
	$(APPLY) $(SRC)/libplacebo/0001-vulkan-blacklist-metal-structs-from-utils_gen.py.patch
	$(APPLY) $(SRC)/libplacebo/0002-pl_thread-use-gettimeofday-for-back-compat.patch
ifdef HAVE_ANDROID
	$(APPLY) $(SRC)/libplacebo/fix-android-build.patch
endif
	$(MOVE)

.libplacebo: libplacebo crossfile.meson
	cd $< && rm -rf ./build
	cd $< && $(HOSTVARS_MESON) $(MESON) $(PLACEBOCONF) build
	cd $< && cd build && ninja install
# Work-around messon issue https://github.com/mesonbuild/meson/issues/4091
	sed -i.orig -e 's/Libs: \(.*\) -L$${libdir} -lplacebo/Libs: -L$${libdir} -lplacebo \1/g' $(PREFIX)/lib/pkgconfig/libplacebo.pc
# Work-around for full paths to static libraries, which libtool does not like
# See https://github.com/mesonbuild/meson/issues/5479
	(cd $(UNPACK_DIR) && $(SRC_BUILT)/pkg-rewrite-absolute.py -i "$(PREFIX)/lib/pkgconfig/libplacebo.pc")
	touch $@
