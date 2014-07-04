# libogg

OGG_VERSION := 1.3.2

OGG_URL := http://downloads.xiph.org/releases/ogg/libogg-$(OGG_VERSION).tar.xz
#OGG_URL := $(CONTRIB_VIDEOLAN)/libogg-$(OGG_VERSION).tar.xz
#OGG_CVSROOT := :pserver:anoncvs@xiph.org:/usr/local/cvsroot

PKGS += ogg
ifeq ($(call need_pkg,"ogg >= 1.0"),)
PKGS_FOUND += ogg
endif

$(TARBALLS)/libogg-$(OGG_VERSION).tar.xz:
	$(call download,$(OGG_URL))

.sum-ogg: libogg-$(OGG_VERSION).tar.xz

libogg: libogg-$(OGG_VERSION).tar.xz .sum-ogg
	$(UNPACK)
	$(APPLY) $(SRC)/ogg/libogg-1.1.patch
	$(APPLY) $(SRC)/ogg/libogg-disable-check.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.ogg: libogg
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
