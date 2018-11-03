# BPG
BPG_VERSION := 0.9.8
BPG_URL := http://bellard.org/bpg/libbpg-$(BPG_VERSION).tar.gz

# default disabled
# PKGS += bpg

$(TARBALLS)/libbpg-$(BPG_VERSION).tar.gz:
	$(call download_pkg,$(BPG_URL),bpg)

.sum-bpg: libbpg-$(BPG_VERSION).tar.gz

bpg: libbpg-$(BPG_VERSION).tar.gz .sum-bpg
	$(UNPACK)
	$(APPLY) $(SRC)/bpg/pic.patch
	$(MOVE)

.bpg: bpg
	cd $< && $(HOSTVARS_PIC) $(MAKE) libbpg.a
	mkdir -p $(PREFIX)/include/ && cp $</libbpg.h $(PREFIX)/include/
	mkdir -p $(PREFIX)/lib/ && cp $</libbpg.a $(PREFIX)/lib/
	touch $@
