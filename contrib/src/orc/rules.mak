# orc

ORC_VERSION := 0.4.18
ORC_URL := $(CONTRIB_VIDEOLAN)/orc/orc-$(ORC_VERSION).tar.gz

ifeq ($(call need_pkg,"orc-0.4"),)
PKGS_FOUND += orc
endif

$(TARBALLS)/orc-$(ORC_VERSION).tar.gz:
	$(call download_pkg,$(ORC_URL),orc)

.sum-orc: orc-$(ORC_VERSION).tar.gz

orc: orc-$(ORC_VERSION).tar.gz .sum-orc
	$(UNPACK)
	$(APPLY) $(SRC)/orc/use-proper-func-detection.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.orc: orc
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
