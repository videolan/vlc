# libmicrodns

LIBMICRODNS_VERSION := 0.2.0
LIBMICRODNS_URL := $(GITHUB)/videolabs/libmicrodns/releases/download/$(LIBMICRODNS_VERSION)/microdns-$(LIBMICRODNS_VERSION).tar.xz

ifndef HAVE_DARWIN_OS
ifdef BUILD_NETWORK
PKGS += microdns
endif
endif
ifeq ($(call need_pkg,"microdns >= 0.1.2"),)
PKGS_FOUND += microdns
endif

$(TARBALLS)/microdns-$(LIBMICRODNS_VERSION).tar.xz:
	$(call download_pkg,$(LIBMICRODNS_URL),microdns)

.sum-microdns: $(TARBALLS)/microdns-$(LIBMICRODNS_VERSION).tar.xz

microdns: microdns-$(LIBMICRODNS_VERSION).tar.xz .sum-microdns
	$(UNPACK)
	$(APPLY) $(SRC)/microdns/0001-mdns-support-XP-Vista-differences-at-runtime.patch
	$(MOVE)

.microdns: microdns crossfile.meson
	$(MESONCLEAN)
	$(MESON) -Dauto_features=disabled
	+$(MESONBUILD)
	touch $@
