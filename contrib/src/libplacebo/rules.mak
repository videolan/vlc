# libplacebo

PLACEBO_VERSION := 0.2.1
PLACEBO_URL := https://github.com/haasn/libplacebo/archive/v$(PLACEBO_VERSION).tar.gz
PLACEBO_ARCHIVE = libplacebo-$(PLACEBO_VERSION).tar.gz

ifdef HAVE_WIN32
LIBPLACEBO_WIN32 = HAVE_WIN32=1
DEPS_libplacebo += pthreads $(DEPS_pthreads)
endif

PKGS += libplacebo
ifeq ($(call need_pkg,"libplacebo"),)
PKGS_FOUND += libplacebo
endif

PLACEBOCONF := --prefix="$(PREFIX)" \
	--libdir lib \
	--default-library static

$(TARBALLS)/$(PLACEBO_ARCHIVE):
	$(call download_pkg,$(PLACEBO_URL),libplacebo)

.sum-libplacebo: $(PLACEBO_ARCHIVE)

libplacebo: $(PLACEBO_ARCHIVE) .sum-libplacebo
	$(UNPACK)
	$(APPLY) $(SRC)/libplacebo/0001-build-use-a-Makefile.patch
	$(MOVE)

.libplacebo: libplacebo
	cd $< && rm -rf ./build
# we don't want to depend on meson/ninja for VLC 3.0
#cd $< && $(HOSTVARS) meson $(PLACEBOCONF) build
#cd $< && cd build && ninja install
	cd $< && $(HOSTVARS_PIC) PREFIX=$(PREFIX) $(LIBPLACEBO_WIN32) make install
	touch $@
