# matroska

MATROSKA_VERSION := 1.4.7
MATROSKA_URL := http://dl.matroska.org/downloads/libmatroska/libmatroska-$(MATROSKA_VERSION).tar.bz2

PKGS += matroska

ifeq ($(call need_pkg,"libmatroska"),)
PKGS_FOUND += matroska
endif

DEPS_matroska = ebml $(DEPS_ebml)

$(TARBALLS)/libmatroska-$(MATROSKA_VERSION).tar.bz2:
	$(call download_pkg,$(MATROSKA_URL),matroska)

.sum-matroska: libmatroska-$(MATROSKA_VERSION).tar.bz2

libmatroska: libmatroska-$(MATROSKA_VERSION).tar.bz2 .sum-matroska
	$(UNPACK)
	$(call pkg_static,"libmatroska.pc.in")
	$(MOVE)

MATROSKA_EXTRA_FLAGS = CXXFLAGS="${CXXFLAGS} -fvisibility=hidden"

.matroska: libmatroska
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(MATROSKA_EXTRA_FLAGS)
	cd $< && $(MAKE) install
	touch $@
