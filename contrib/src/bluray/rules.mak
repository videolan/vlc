# LIBBLURAY

ifdef BUILD_DISCS
PKGS += libbluray
endif
ifeq ($(call need_pkg,"libbluray >= 0.2.1"),)
PKGS_FOUND += libbluray
endif

BLURAY_VERSION := 0.2.1
BLURAY_URL := http://ftp.videolan.org/pub/videolan/libbluray/0.2.1/libbluray-0.2.1.tar.bz2

$(TARBALLS)/libbluray-0.2.1.tar.bz2:
	$(call download,$(BLURAY_URL))

.sum-bluray: libbluray-0.2.1.tar.bz2

libbluray: libbluray-0.2.1.tar.bz2 .sum-bluray
	$(UNPACK)
	$(APPLY) $(SRC)/bluray/pkg-static.patch
	$(MOVE)

.libbluray: libbluray
	cd $< && ./bootstrap
	cd $< && $(HOSTVARS) ./configure --disable-examples --disable-debug --disable-libxml2 $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
