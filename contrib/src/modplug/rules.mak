# modplug

#MODPLUG_VERSION := 0.8.8.4
#MODPLUG_URL := $(SF)/modplug-xmms/libmodplug-$(MODPLUG_VERSION).tar.gz

MODPLUG_GIT_HASH := bc8cb8248788c05b77da7d653f4c677354339a21
#MODPLUG_URL := http://sourceforge.net/code-snapshots/git/m/mo/modplug-xmms/git.git/modplug-xmms-git-$(MODPLUG_GIT_HASH).zip
MODPLUG_URL := http://download.videolan.org/pub/contrib/modplug-xmms-git-$(MODPLUG_GIT_HASH).zip

PKGS += modplug
ifeq ($(call need_pkg,"libmodplug >= 0.8.4 libmodplug != 0.8.8"),)
PKGS_FOUND += modplug
endif

$(TARBALLS)/modplug-xmms-git-$(MODPLUG_GIT_HASH).zip:
	$(call download,$(MODPLUG_URL))

.sum-modplug: modplug-xmms-git-$(MODPLUG_GIT_HASH).zip

libmodplug: modplug-xmms-git-$(MODPLUG_GIT_HASH).zip .sum-modplug
	$(UNPACK)
	$(call pkg_static,"libmodplug/libmodplug.pc.in")
	$(MOVE)

.modplug: libmodplug
	cd $< && $(RECONF)
	cd $</libmodplug && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $</libmodplug && $(MAKE) install
	touch $@
