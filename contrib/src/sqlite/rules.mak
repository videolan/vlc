# sqlite

SQLITE_VERSION := 3530400
SQLITE_URL := https://www.sqlite.org/2026/sqlite-autoconf-$(SQLITE_VERSION).tar.gz

ifeq ($(call need_pkg,"sqlite3 >= 3.33.0"),)
PKGS_FOUND += sqlite
endif

SQLITE_CONF = --prefix="$(PREFIX)" --build="$(BUILD)" --host="$(HOST)" \
	--disable-shared --disable-rpath \
	--disable-readline

ifdef HAVE_WINSTORE
SQLITE_CONF += CFLAGS="$(CFLAGS) -DSQLITE_OS_WINRT=1"
endif

$(TARBALLS)/sqlite-autoconf-$(SQLITE_VERSION).tar.gz:
	$(call download_pkg,$(SQLITE_URL),sqlite)

.sum-sqlite: sqlite-autoconf-$(SQLITE_VERSION).tar.gz

sqlite: sqlite-autoconf-$(SQLITE_VERSION).tar.gz .sum-sqlite
	$(UNPACK)
	$(call pkg_static, "sqlite3.pc.in")
	$(MOVE)

.sqlite: sqlite
	$(MAKEBUILDDIR)
	$(MAKECONFDIR)/configure $(SQLITE_CONF)
	+$(MAKEBUILD) bin_PROGRAMS=
	+$(MAKEBUILD) bin_PROGRAMS= install
	touch $@
