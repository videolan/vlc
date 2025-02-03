# dvbpsi

DVBPSI_VERSION := 1.3.3
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
	$(call update_autoconfig,.auto)
	$(APPLY) $(SRC)/dvbpsi/dvbpsi-noexamples.patch
	$(APPLY) $(SRC)/dvbpsi/dvbpsi-sys-types.patch
	$(APPLY) $(SRC)/dvbpsi/0001-really-identify-duplicates.patch
	$(APPLY) $(SRC)/dvbpsi/0002-really-reset-packet-counter.patch
	$(APPLY) $(SRC)/dvbpsi/0001-dvbpsi_packet_push-compute-sizes-using-pointer-to-en.patch
	$(APPLY) $(SRC)/dvbpsi/0002-dvbpsi_packet_push-check-adaptation-field-length.patch
	$(APPLY) $(SRC)/dvbpsi/0003-dvbpsi_packet_push-check-section-pointers-field.patch
	$(APPLY) $(SRC)/dvbpsi/0004-dvbpsi_packet_push-check-section-length.patch
	$(MOVE)

.dvbpsi: libdvbpsi
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
