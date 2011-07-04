# schroedinger

SCHROEDINGER_VERSION := 1.0.10

SCHROEDINGER_URL := http://diracvideo.org/download/schroedinger/schroedinger-$(SCHROEDINGER_VERSION).tar.gz

NEED_SCHROEDINGER = $(call need_pkg,"schroedinger-1.0")

$(TARBALLS)/schroedinger-$(SCHROEDINGER_VERSION).tar.gz:
	$(call download,$(SCHROEDINGER_URL))

.sum-schroedinger: schroedinger-$(SCHROEDINGER_VERSION).tar.gz

schroedinger: schroedinger-$(SCHROEDINGER_VERSION).tar.gz .sum-schroedinger
	$(UNPACK)
	$(APPLY) $(SRC)/schroedinger/schroedinger-notests.patch
	$(MOVE)

ifeq ($(NEED_SCHROEDINGER),)
.schroedinger:
else
PKGS += schroedinger

.schroedinger: schroedinger
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure --with-thread=none --disable-gtk-doc $(HOSTCONF)
	cd $< && $(MAKE) install
endif
	touch $@
