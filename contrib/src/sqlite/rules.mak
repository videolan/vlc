# sqlite

SQLITE_VERSION := 3530400
SQLITE_URL := https://www.sqlite.org/2026/sqlite-autoconf-$(SQLITE_VERSION).tar.gz

ifeq ($(call need_pkg,"sqlite3 >= 3.33.0"),)
PKGS_FOUND += sqlite
endif

SQLITE_CONF = --prefix="$(PREFIX)" --build="$(BUILD)" --host="$(HOST)" \
	--disable-shared --disable-rpath \
	--disable-readline

# force build FTS3 used by the medialibrary, otherwise it may crash
SQLITE_CONF += --enable-fts3

$(TARBALLS)/sqlite-autoconf-$(SQLITE_VERSION).tar.gz:
ifdef HAVE_WINSTORE
	$(error "sqlite no longer supported in UWP")
endif
	$(call download_pkg,$(SQLITE_URL),sqlite)

.sum-sqlite: sqlite-autoconf-$(SQLITE_VERSION).tar.gz

sqlite: sqlite-autoconf-$(SQLITE_VERSION).tar.gz .sum-sqlite
ifdef HAVE_WINSTORE
	$(error "sqlite no longer supported in UWP")
endif
	$(UNPACK)
	$(call pkg_static, "sqlite3.pc.in")
	$(MOVE)

.sqlite: sqlite
	$(MAKEBUILDDIR)
	$(MAKECONFDIR)/configure $(SQLITE_CONF)
	+$(MAKEBUILD) bin_PROGRAMS=
	+$(MAKEBUILD) bin_PROGRAMS= install
	touch $@
