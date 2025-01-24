# dvbcsa

DVBCSA_VERSION := 1.1.0
DVBCSA_URL := $(VIDEOLAN)/libdvbcsa/$(DVBCSA_VERSION)/libdvbcsa-$(DVBCSA_VERSION).tar.gz

ifdef GPL
PKGS += dvbcsa
endif
ifeq ($(call need_pkg,"libdvbcsa >= 1.1.0"),)
PKGS_FOUND += dvbcsa
endif

$(TARBALLS)/libdvbcsa-$(DVBCSA_VERSION).tar.gz:
	$(call download,$(DVBCSA_URL))

.sum-dvbcsa: libdvbcsa-$(DVBCSA_VERSION).tar.gz

libdvbcsa: libdvbcsa-$(DVBCSA_VERSION).tar.gz .sum-dvbcsa
	$(UNPACK)
	# $(call update_autoconfig,.)
	$(APPLY) $(SRC)/dvbcsa/0001-generate-pkgconfig.patch
	$(MOVE)

.dvbcsa: libdvbcsa
	$(REQUIRE_GPL)
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE)
	+$(MAKEBUILD) -C src
	+$(MAKEBUILD) -C src install
	touch $@
