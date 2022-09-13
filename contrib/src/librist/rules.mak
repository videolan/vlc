# librist

LIBRIST_VERSION := v0.2.7
LIBRIST_URL := http://code.videolan.org/rist/librist/-/archive/$(LIBRIST_VERSION)/librist-$(LIBRIST_VERSION).tar.gz

ifdef BUILD_NETWORK
PKGS += librist
endif

DEPS_librist =
ifdef HAVE_WIN32
DEPS_librist += pthreads $(DEPS_pthreads)
endif

ifeq ($(call need_pkg,"librist >= 0.2"),)
PKGS_FOUND += librist
endif

LIBRIST_CONF = -Dbuilt_tools=false -Dtest=false
ifdef HAVE_WIN32
LIBRIST_CONF += -Dhave_mingw_pthreads=true
endif

$(TARBALLS)/librist-$(LIBRIST_VERSION).tar.gz:
	$(call download_pkg,$(LIBRIST_URL),librist)

.sum-librist: librist-$(LIBRIST_VERSION).tar.gz

librist: librist-$(LIBRIST_VERSION).tar.gz .sum-librist
	$(UNPACK)
	$(APPLY) $(SRC)/librist/librist-fix-libcjson-meson.patch
	$(APPLY) $(SRC)/librist/win32-timing.patch
	$(MOVE)

.librist: librist crossfile.meson
	rm -rf $</build
	$(HOSTVARS_MESON) $(MESON) $(LIBRIST_CONF) $</build $<
	meson install -C $</build
	touch $@
