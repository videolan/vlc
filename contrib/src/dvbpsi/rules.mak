# dvbpsi

DVBPSI_VERSION := 0.2.0
DVBPSI_URL := $(VIDEOLAN)/libdvbpsi/$(DVBPSI_VERSION)/libdvbpsi-$(DVBPSI_VERSION).tar.bz2

PKGS += dvbpsi

$(TARBALLS)/libdvbpsi-$(DVBPSI_VERSION).tar.bz2:
	$(call download,$(DVBPSI_URL))

.sum-dvbpsi: libdvbpsi-$(DVBPSI_VERSION).tar.bz2

libdvbpsi: libdvbpsi-$(DVBPSI_VERSION).tar.bz2 .sum-dvbpsi
	$(UNPACK)
	$(APPLY) $(SRC)/dvbpsi/libdvbpsi-example.patch
	$(MOVE)

.dvbpsi: libdvbpsi
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --enable-release
	cd $< && $(MAKE) install
	touch $@
