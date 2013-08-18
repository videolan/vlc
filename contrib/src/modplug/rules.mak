# modplug

#MODPLUG_VERSION := 0.8.8.4
#MODPLUG_URL := $(SF)/modplug-xmms/libmodplug-$(MODPLUG_VERSION).tar.gz

MODPLUG_GIT_HASH := 9b08cc646c3dc94dd446ab0671e3427dae8a83fc
MODPLUG_URL := http://sourceforge.net/code-snapshots/git/m/mo/modplug-xmms/git.git/modplug-xmms-git-$(MODPLUG_GIT_HASH).zip

PKGS += modplug
ifeq ($(call need_pkg,"libmodplug >= 0.8.4 libmodplug != 0.8.8"),)
PKGS_FOUND += modplug
endif

$(TARBALLS)/modplug-xmms-git-$(MODPLUG_GIT_HASH).zip:
	$(call download,$(MODPLUG_URL))

.sum-modplug: modplug-xmms-git-$(MODPLUG_GIT_HASH).zip

libmodplug: modplug-xmms-git-$(MODPLUG_GIT_HASH).zip .sum-modplug
	$(UNPACK)
	$(APPLY) $(SRC)/modplug/long.patch
	$(call pkg_static,"libmodplug/libmodplug.pc.in")
	$(MOVE)

.modplug: libmodplug
	cd $< && $(RECONF)
	cd $</libmodplug && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $</libmodplug && $(MAKE) install
	touch $@
