# modplug

MODPLUG_VERSION := 0.8.8.5
MODPLUG_URL := $(SF)/modplug-xmms/libmodplug-$(MODPLUG_VERSION).tar.gz

PKGS += modplug
ifeq ($(call need_pkg,"libmodplug >= 0.8.8.5"),)
PKGS_FOUND += modplug
endif

$(TARBALLS)/libmodplug-$(MODPLUG_VERSION).tar.gz:
	$(call download_pkg,$(MODPLUG_URL),modplug)

.sum-modplug: libmodplug-$(MODPLUG_VERSION).tar.gz

libmodplug: libmodplug-$(MODPLUG_VERSION).tar.gz .sum-modplug
	$(UNPACK)
	$(APPLY) $(SRC)/modplug/modplug-win32-static.patch
	$(call pkg_static,"libmodplug.pc.in")
	$(MOVE)

.modplug: libmodplug
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
