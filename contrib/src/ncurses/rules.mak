# ncurses

NCURSES_VERSION := 6.5
NCURSES_URL := $(GNU)/ncurses/ncurses-$(NCURSES_VERSION).tar.gz

ifdef HAVE_MACOSX
PKGS += ncurses
endif

ifeq ($(call need_pkg,"ncursesw"),)
PKGS_FOUND += ncurses
endif

$(TARBALLS)/ncurses-$(NCURSES_VERSION).tar.gz:
	$(call download_pkg,$(NCURSES_URL),ncurses)

.sum-ncurses: ncurses-$(NCURSES_VERSION).tar.gz

ncurses: ncurses-$(NCURSES_VERSION).tar.gz .sum-ncurses
	$(UNPACK)
	$(call update_autoconfig,.)
	$(APPLY) $(SRC)/ncurses/ncurses-win32.patch
	$(MOVE)

NCURSES_CONF := --enable-widec --with-terminfo-dirs=/usr/share/terminfo \
    --with-pkg-config=yes --enable-pc-files --without-manpages --without-tests \
    --without-ada --without-progs
ifdef WITH_OPTIMIZATION
NCURSES_CONF+= --without-debug
endif
ifdef HAVE_WIN32
NCURSES_CONF+= --disable-sigwinch
endif
ifdef HAVE_MACOSX
NCURSES_CONF += cf_cv_func_clock_gettime=no
endif

.ncurses: ncurses
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(NCURSES_CONF)
	+$(MAKEBUILD) -C ncurses -j1
	+$(MAKEBUILD) -C ncurses install
	+$(MAKEBUILD) -C include -j1
	+$(MAKEBUILD) -C include install
	+$(MAKEBUILD) -C misc pc-files
	install $(BUILD_DIR)/misc/ncursesw.pc "$(PREFIX)/lib/pkgconfig"
	touch $@
