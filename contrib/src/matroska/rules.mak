# matroska

MATROSKA_VERSION := 1.4.2
MATROSKA_URL := http://dl.matroska.org/downloads/libmatroska/libmatroska-$(MATROSKA_VERSION).tar.bz2
#MATROSKA_URL := $(CONTRIB_VIDEOLAN)/libmatroska-$(MATROSKA_VERSION).tar.bz2

PKGS += matroska
DEPS_matroska = ebml $(DEPS_ebml)

$(TARBALLS)/libmatroska-$(MATROSKA_VERSION).tar.bz2:
	$(call download,$(MATROSKA_URL))

.sum-matroska: libmatroska-$(MATROSKA_VERSION).tar.bz2

libmatroska: libmatroska-$(MATROSKA_VERSION).tar.bz2 .sum-matroska
	$(UNPACK)
	$(MOVE)

MATROSKA_EXTRA_FLAGS = CXXFLAGS="${CXXFLAGS} -fvisibility=hidden"

.matroska: libmatroska
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(MATROSKA_EXTRA_FLAGS)
	cd $< && $(MAKE) install
	touch $@
