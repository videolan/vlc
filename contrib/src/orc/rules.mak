# orc

ORC_VERSION := 0.4.18

ORC_URL := http://download.videolan.org/pub/contrib/orc-$(ORC_VERSION).tar.gz

ifeq ($(call need_pkg,"orc-0.4"),)
PKGS_FOUND += orc
endif

$(TARBALLS)/orc-$(ORC_VERSION).tar.gz:
	$(call download,$(ORC_URL))

.sum-orc: orc-$(ORC_VERSION).tar.gz

orc: orc-$(ORC_VERSION).tar.gz .sum-orc
	$(UNPACK)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.orc: orc
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
