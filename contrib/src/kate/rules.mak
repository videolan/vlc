# Kate

KATE_VERSION := 0.4.1
KATE_URL := http://libkate.googlecode.com/files/libkate-$(KATE_VERSION).tar.gz

PKGS += kate
ifeq ($(call need_pkg,"kate >= 0.1.5"),)
PKGS_FOUND += kate
endif

$(TARBALLS)/libkate-$(KATE_VERSION).tar.gz:
	$(call download_pkg,$(KATE_URL),kate)

.sum-kate: libkate-$(KATE_VERSION).tar.gz

libkate: libkate-$(KATE_VERSION).tar.gz .sum-kate
	$(UNPACK)
	$(UPDATE_AUTOCONFIG) && cd $(UNPACK_DIR) && mv config.guess config.sub misc/autotools
	$(MOVE)

DEPS_kate = ogg $(DEPS_ogg)

KATE_CONF := --disable-valgrind --disable-doc

.kate: libkate
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(KATE_CONF)
	$(MAKE) -C $< SUBDIRS=. install
	touch $@
