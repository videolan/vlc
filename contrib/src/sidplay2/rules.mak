# sidplay2

SID_VERSION := 2.1.1
SID_URL := $(SF)/sidplay2/sidplay2/sidplay-libs-$(SID_VERSION)/sidplay-libs-$(SID_VERSION).tar.gz

ifdef GPL
PKGS += sidplay2
endif

$(TARBALLS)/sidplay-libs-$(SID_VERSION).tar.gz:
	$(call download,$(SID_URL))

.sum-sidplay2: sidplay-libs-$(SID_VERSION).tar.gz

sidplay-libs: sidplay-libs-$(SID_VERSION).tar.gz .sum-sidplay2
	$(UNPACK)
	$(APPLY) $(SRC)/sidplay2/sidplay2-openmode.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-endian.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-smartprt.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-noutils.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-string.patch
	$(MOVE)

.sidplay2: sidplay-libs
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	cp -- $(PREFIX)/lib/sidplay/builders/* "$(PREFIX)/lib/"
	touch $@
