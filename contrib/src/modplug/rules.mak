# modplug

MODPLUG_VERSION := 0.8.8.3
MODPLUG_URL := $(SF)/modplug-xmms/libmodplug-$(MODPLUG_VERSION).tar.gz

PKGS += modplug

$(TARBALLS)/libmodplug-$(MODPLUG_VERSION).tar.gz:
	$(call download,$(MODPLUG_URL))

.sum-modplug: libmodplug-$(MODPLUG_VERSION).tar.gz

libmodplug: libmodplug-$(MODPLUG_VERSION).tar.gz .sum-modplug
	$(UNPACK)
	$(MOVE)

.modplug: libmodplug
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
