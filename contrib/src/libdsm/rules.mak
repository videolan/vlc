# libdsm

LIBDSM_VERSION := 0.4.3
LIBDSM_URL := $(GITHUB)/videolabs/libdsm/releases/download/v$(LIBDSM_VERSION)/libdsm-$(LIBDSM_VERSION).tar.xz

ifeq ($(call need_pkg,"libdsm >= 0.2.0"),)
PKGS_FOUND += libdsm
endif

$(TARBALLS)/libdsm-$(LIBDSM_VERSION).tar.xz:
	$(call download_pkg,$(LIBDSM_URL),libdsm)

.sum-libdsm: libdsm-$(LIBDSM_VERSION).tar.xz

libdsm: libdsm-$(LIBDSM_VERSION).tar.xz .sum-libdsm
	$(UNPACK)
	$(APPLY) $(SRC)/libdsm/0001-Avoid-relying-on-implicit-function-declarations.patch
	$(APPLY) $(SRC)/libdsm/0001-use-GetCurrentProcessId-for-the-process-ID-on-Window.patch
	$(MOVE)

DEPS_libdsm = libtasn1 $(DEPS_libtasn1) iconv $(DEPS_iconv)
ifdef HAVE_WIN32
DEPS_libdsm += winpthreads $(DEPS_winpthreads)
endif

.libdsm: libdsm crossfile.meson
	$(MESONCLEAN)
	$(MESON) -Dauto_features=disabled -Dbinaries=false
	+$(MESONBUILD)
	touch $@
