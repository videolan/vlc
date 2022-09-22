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

NCURSES_CONF := --enable-widec --without-shared --with-terminfo-dirs=/usr/share/terminfo \
    --with-pkg-config=yes --enable-pc-files
ifdef WITH_OPTIMIZATION
NCURSES_CONF+= --without-debug
endif

.ncurses: ncurses
	cd $< && mkdir -p "$(PREFIX)/lib/pkgconfig" && $(HOSTVARS) PKG_CONFIG_LIBDIR="$(PREFIX)/lib/pkgconfig" ./configure $(patsubst --datarootdir=%,,$(HOSTCONF)) $(NCURSES_CONF)
	cd $< && $(MAKE) -C ncurses -j1 && $(MAKE) -C ncurses install
	cd $< && $(MAKE) -C include -j1 && $(MAKE) -C include install
	cd $< && $(MAKE) -C misc pc-files && cp misc/ncursesw.pc "$(PREFIX)/lib/pkgconfig"
	touch $@
