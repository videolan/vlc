# dvbpsi

DVBPSI_VERSION := 1.3.2
DVBPSI_URL := $(VIDEOLAN)/libdvbpsi/$(DVBPSI_VERSION)/libdvbpsi-$(DVBPSI_VERSION).tar.bz2

PKGS += dvbpsi
ifeq ($(call need_pkg,"libdvbpsi >= 1.2.0"),)
PKGS_FOUND += dvbpsi
endif

$(TARBALLS)/libdvbpsi-$(DVBPSI_VERSION).tar.bz2:
	$(call download,$(DVBPSI_URL))

.sum-dvbpsi: libdvbpsi-$(DVBPSI_VERSION).tar.bz2

libdvbpsi: libdvbpsi-$(DVBPSI_VERSION).tar.bz2 .sum-dvbpsi
	$(UNPACK)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub .auto
	$(APPLY) $(SRC)/dvbpsi/dvbpsi-noexamples.patch
	$(APPLY) $(SRC)/dvbpsi/dvbpsi-sys-types.patch
	$(APPLY) $(SRC)/dvbpsi/0001-really-identify-duplicates.patch
	$(APPLY) $(SRC)/dvbpsi/0002-really-reset-packet-counter.patch
	$(APPLY) $(SRC)/dvbpsi/0001-really-set-last-section-number.patch
	$(MOVE)

.dvbpsi: libdvbpsi
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
