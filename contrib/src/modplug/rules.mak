# modplug

MODPLUG_VERSION := 0.8.8.4
MODPLUG_URL := $(SF)/modplug-xmms/libmodplug-$(MODPLUG_VERSION).tar.gz

PKGS += modplug
ifeq ($(call need_pkg,"libmodplug >= 0.8.4 libmodplug != 0.8.8"),)
PKGS_FOUND += modplug
endif

$(TARBALLS)/libmodplug-$(MODPLUG_VERSION).tar.gz:
	$(call download,$(MODPLUG_URL))

.sum-modplug: libmodplug-$(MODPLUG_VERSION).tar.gz

libmodplug: libmodplug-$(MODPLUG_VERSION).tar.gz .sum-modplug
	$(UNPACK)
	$(MOVE)

.modplug: libmodplug
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
