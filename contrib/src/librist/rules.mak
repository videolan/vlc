# librist

LIBRIST_VERSION := v0.2.15
LIBRIST_URL := http://code.videolan.org/rist/librist/-/archive/$(LIBRIST_VERSION)/librist-$(LIBRIST_VERSION).tar.gz

ifdef BUILD_NETWORK
PKGS += librist
endif

DEPS_librist =
ifdef HAVE_WIN32
DEPS_librist += winpthreads $(DEPS_winpthreads)
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
	$(MOVE)

.librist: librist crossfile.meson
	$(MESONCLEAN)
	$(MESON) $(LIBRIST_CONF)
	+$(MESONBUILD)
	touch $@
