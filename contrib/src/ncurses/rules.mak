# ncurses

NCURSES_VERSION := 5.9
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

.ncurses: ncurses
	cd $< && mkdir -p "$(PREFIX)/lib/pkgconfig" && $(HOSTVARS) PKG_CONFIG_LIBDIR="$(PREFIX)/lib/pkgconfig" ./configure $(patsubst --datarootdir=%,,$(HOSTCONF)) --without-debug --enable-widec --without-develop --without-shared --with-terminfo-dirs=/usr/share/terminfo --with-pkg-config=yes --enable-pc-files
	cd $</ncurses && make -j1 && make install
	cd $</include && make -j1 && make install
	cd $</misc && make pc-files && cp ncursesw.pc "$(PREFIX)/lib/pkgconfig"
	touch $@
