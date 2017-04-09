# jpeg

OPENJPEG_VERSION := version.1.5.2
OPENJPEG_URL := https://github.com/uclouvain/openjpeg/archive/$(OPENJPEG_VERSION).tar.gz

$(TARBALLS)/openjpeg-$(OPENJPEG_VERSION).tar.gz:
	$(call download_pkg,$(OPENJPEG_URL),openjpeg)

.sum-openjpeg: openjpeg-$(OPENJPEG_VERSION).tar.gz

openjpeg: openjpeg-$(OPENJPEG_VERSION).tar.gz .sum-openjpeg
	$(UNPACK)
ifdef HAVE_VISUALSTUDIO
	$(APPLY) $(SRC)/openjpeg/msvc.patch
endif
	$(APPLY) $(SRC)/openjpeg/restrict.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.openjpeg: openjpeg
	$(RECONF)
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -DOPJ_STATIC" ./configure --enable-png=no --enable-tiff=no $(HOSTCONF)
	cd $< && $(MAKE) -C libopenjpeg -j1 install
	cd $< && ../../../contrib/src/pkg-static.sh libopenjpeg1.pc
	cd $< && $(MAKE) install-data
	touch $@
