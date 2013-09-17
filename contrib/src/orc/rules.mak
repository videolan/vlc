# orc

ORC_VERSION := 0.4.18

ORC_URL := http://code.entropywave.com/download/orc/orc-$(ORC_VERSION).tar.gz

ifeq ($(call need_pkg,"orc-0.4"),)
PKGS_FOUND += orc
endif

$(TARBALLS)/orc-$(ORC_VERSION).tar.gz:
	$(call download,$(ORC_URL))

.sum-orc: orc-$(ORC_VERSION).tar.gz

orc: orc-$(ORC_VERSION).tar.gz .sum-orc
	$(UNPACK)
	$(APPLY) $(SRC)/orc/android.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.orc: orc
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
