# goom

GOOM_VERSION := 2k4-0
GOOM_URL := $(CONTRIB_VIDEOLAN)/goom/goom-$(GOOM_VERSION)-src.tar.gz

PKGS += goom
ifeq ($(call need_pkg,"libgoom2"),)
PKGS_FOUND += goom
endif

$(TARBALLS)/goom-$(GOOM_VERSION)-src.tar.gz:
	$(call download,$(GOOM_URL))

.sum-goom: goom-$(GOOM_VERSION)-src.tar.gz

goom: goom-$(GOOM_VERSION)-src.tar.gz .sum-goom
	$(UNPACK)
	mv goom2k4-0 goom-2k4-0-src
	$(APPLY) $(SRC)/goom/goom2k4-0-memleaks.patch
	$(APPLY) $(SRC)/goom/goom2k4-autotools.patch
	$(APPLY) $(SRC)/goom/goom2k4-noxmmx.patch
	$(APPLY) $(SRC)/goom/goom2k4-xmmslibdir.patch
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/goom/goom2k4-0-win32.patch
endif
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/goom/goom2k4-osx.patch
endif
	$(MOVE)

.goom: goom
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-glibtest --disable-gtktest
	cd $< && $(MAKE) install
	touch $@
