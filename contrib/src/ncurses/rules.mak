# ncurses

NCURSES_VERSION := 6.3
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
	$(MOVE)

NCURSES_CONF := --enable-widec --with-terminfo-dirs=/usr/share/terminfo \
    --with-pkg-config=yes --enable-pc-files --without-manpages --without-tests \
    --without-ada --without-progs
ifdef WITH_OPTIMIZATION
NCURSES_CONF+= --without-debug
endif

.ncurses: ncurses
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(NCURSES_CONF)
	+$(MAKEBUILD) -C ncurses -j1
	+$(MAKEBUILD) -C ncurses install
	+$(MAKEBUILD) -C include -j1
	+$(MAKEBUILD) -C include install
	+$(MAKEBUILD) -C misc pc-files
	install $</_build/misc/ncursesw.pc "$(PREFIX)/lib/pkgconfig"
	touch $@
