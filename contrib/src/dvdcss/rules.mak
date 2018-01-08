# DVDCSS
DVDCSS_VERSION := 1.4.1
DVDCSS_URL := $(VIDEOLAN)/libdvdcss/$(DVDCSS_VERSION)/libdvdcss-$(DVDCSS_VERSION).tar.bz2

ifeq ($(call need_pkg,"libdvdcss"),)
PKGS_FOUND += dvdcss
endif

$(TARBALLS)/libdvdcss-$(DVDCSS_VERSION).tar.bz2:
	$(call download,$(DVDCSS_URL))

.sum-dvdcss: libdvdcss-$(DVDCSS_VERSION).tar.bz2


dvdcss: libdvdcss-$(DVDCSS_VERSION).tar.bz2 .sum-dvdcss
	$(UNPACK)
	$(MOVE)

.dvdcss: dvdcss
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure --disable-doc $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
