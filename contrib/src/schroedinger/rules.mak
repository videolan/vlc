# schroedinger

SCHROEDINGER_VERSION := 1.0.11
SCHROEDINGER_URL := http://diracvideo.org/download/schroedinger/schroedinger-$(SCHROEDINGER_VERSION).tar.gz

PKGS += schroedinger
ifeq ($(call need_pkg,"schroedinger-1.0"),)
PKGS_FOUND += schroedinger
endif

$(TARBALLS)/schroedinger-$(SCHROEDINGER_VERSION).tar.gz:
	$(call download,$(SCHROEDINGER_URL))

.sum-schroedinger: schroedinger-$(SCHROEDINGER_VERSION).tar.gz

schroedinger: schroedinger-$(SCHROEDINGER_VERSION).tar.gz .sum-schroedinger
	$(UNPACK)
	$(APPLY) $(SRC)/schroedinger/schroedinger-notests.patch
	$(APPLY) $(SRC)/schroedinger/android.patch
	$(MOVE)

DEPS_schroedinger = orc $(DEPS_orc)

.schroedinger: schroedinger
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure --with-thread=none --disable-gtk-doc $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
