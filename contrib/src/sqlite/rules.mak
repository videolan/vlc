# sqlite

SQLITE_VERSION := 3340100
SQLITE_URL := https://www.sqlite.org/2021/sqlite-autoconf-$(SQLITE_VERSION).tar.gz

PKGS += sqlite

ifeq ($(call need_pkg,"sqlite3 >= 3.33.0"),)
PKGS_FOUND += sqlite
endif

SQLITE_CONF = --disable-readline --disable-shell

ifdef HAVE_WINSTORE
SQLITE_CONF += CFLAGS="$(CFLAGS) -DSQLITE_OS_WINRT=1"
endif

$(TARBALLS)/sqlite-autoconf-$(SQLITE_VERSION).tar.gz:
	$(call download_pkg,$(SQLITE_URL),sqlite)

.sum-sqlite: sqlite-autoconf-$(SQLITE_VERSION).tar.gz

sqlite: sqlite-autoconf-$(SQLITE_VERSION).tar.gz .sum-sqlite
	$(UNPACK)
	$(APPLY) $(SRC)/sqlite/sqlite-no-shell.patch
	$(call pkg_static, "sqlite3.pc.in")
	$(MOVE)

.sqlite: sqlite
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(SQLITE_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
