# orc

ORC_VERSION := 0.4.14

ORC_URL := http://code.entropywave.com/download/orc/orc-$(ORC_VERSION).tar.gz

NEED_ORC = $(call need_pkg,"orc-0.4")

$(TARBALLS)/orc-$(ORC_VERSION).tar.gz:
	$(call download,$(ORC_URL))

.sum-orc: orc-$(ORC_VERSION).tar.gz

orc: orc-$(ORC_VERSION).tar.gz .sum-orc
	$(UNPACK)
	$(APPLY) $(SRC)/orc/orc-stdint.patch
	$(MOVE)

ifeq ($(NEED_ORC),)
.orc:
else
PKGS += orc

.orc: orc
	#$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
endif
	touch $@
