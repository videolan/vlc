# jpeg

OPENJPEG_VERSION := 1_4_sources_r697
OPENJPEG_URL := http://openjpeg.googlecode.com/files/openjpeg_v$(OPENJPEG_VERSION).tgz

$(TARBALLS)/openjpeg_v$(OPENJPEG_VERSION).tgz:
	$(call download,$(OPENJPEG_URL))

.sum-openjpeg: openjpeg_v$(OPENJPEG_VERSION).tgz

openjpeg: openjpeg_v$(OPENJPEG_VERSION).tgz .sum-openjpeg
	$(UNPACK)
	$(APPLY) $(SRC)/openjpeg/pkg-config.patch
	$(MOVE)

.openjpeg: openjpeg
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure --enable-png=no --enable-tiff=no $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
