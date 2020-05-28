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

# goom2k4-0-src unpacks into a dir named goom2k4-0
goom: UNPACK_DIR=goom2k4-0
goom: goom-$(GOOM_VERSION)-src.tar.gz .sum-goom
	$(UNPACK)
	$(APPLY) $(SRC)/goom/goom2k4-0-memleaks.patch
	$(APPLY) $(SRC)/goom/goom2k4-autotools.patch
	$(APPLY) $(SRC)/goom/goom2k4-noxmmx.patch
	$(APPLY) $(SRC)/goom/goom2k4-xmmslibdir.patch
ifdef HAVE_WIN32
ifdef MSYS_BUILD
	unix2dos $(SRC)/goom/goom2k4-0-win32.patch
endif
	$(APPLY) $(SRC)/goom/goom2k4-0-win32.patch
endif
ifdef HAVE_MACOSX
	$(APPLY) $(SRC)/goom/goom2k4-osx.patch
endif
	$(APPLY) $(SRC)/goom/clang-emms.patch
	$(MOVE)

.goom: goom
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-glibtest --disable-gtktest
	cd $< && $(MAKE) install
	touch $@
