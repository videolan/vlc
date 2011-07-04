# Tiger

TIGER_VERSION := 0.3.1
TIGER_URL := http://libtiger.googlecode.com/files/libtiger-$(TIGER_VERSION).tar.gz

ifeq ($(call need_pkg,"pangocairo >= 0.16"),)
# only available if the system has pangocairo
PKGS += tiger
endif
ifeq ($(call need_pkg,"tiger >= 0.3.1"),)
PKGS_FOUND += tiger
endif

$(TARBALLS)/libtiger-$(TIGER_VERSION).tar.gz:
	$(call download,$(TIGER_URL))

.sum-tiger: libtiger-$(TIGER_VERSION).tar.gz

libtiger: libtiger-$(TIGER_VERSION).tar.gz .sum-tiger
	$(UNPACK)
	$(MOVE)

DEPS_tiger = kate $(DEPS_kate)

.tiger: libtiger
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-doc
	cd $< && $(MAKE) install
	touch $@
