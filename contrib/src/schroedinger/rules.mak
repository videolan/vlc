# schroedinger

SCHROEDINGER_VERSION := 1.0.11
SCHROEDINGER_URL := $(CONTRIB_VIDEOLAN)/schroedinger/schroedinger-$(SCHROEDINGER_VERSION).tar.gz

PKGS += schroedinger
ifeq ($(call need_pkg,"schroedinger-1.0"),)
PKGS_FOUND += schroedinger
endif

$(TARBALLS)/schroedinger-$(SCHROEDINGER_VERSION).tar.gz:
	$(call download,$(SCHROEDINGER_URL),schroedinger)

.sum-schroedinger: schroedinger-$(SCHROEDINGER_VERSION).tar.gz

schroedinger: schroedinger-$(SCHROEDINGER_VERSION).tar.gz .sum-schroedinger
	$(UNPACK)
	$(APPLY) $(SRC)/schroedinger/schroedinger-notests.patch
	$(call pkg_static,"schroedinger.pc.in")
	$(MOVE)

DEPS_schroedinger = orc $(DEPS_orc)

SCHRODINGER_CONF := --with-thread=none --disable-gtk-doc

.schroedinger: schroedinger
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(SCHRODINGER_CONF)
	$(MAKE) -C $< install
	touch $@
