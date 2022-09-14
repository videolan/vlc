# sqlite

SQLITE_VERSION := 3340100
SQLITE_URL := https://www.sqlite.org/2021/sqlite-autoconf-$(SQLITE_VERSION).tar.gz

PKGS += sqlite

ifeq ($(call need_pkg,"sqlite3 >= 3.33.0"),)
PKGS_FOUND += sqlite
endif

SQLITE_CFLAGS := $(CFLAGS)
ifdef HAVE_WINSTORE
SQLITE_CFLAGS += -DSQLITE_OS_WINRT=1
endif

SQLITE_CONF = --disable-readline --disable-shell

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
	cd $< && $(HOSTVARS) CFLAGS="$(SQLITE_CFLAGS)" ./configure $(HOSTCONF) $(SQLITE_CONF)
	cd $< && $(MAKE) && $(MAKE) install
	touch $@
