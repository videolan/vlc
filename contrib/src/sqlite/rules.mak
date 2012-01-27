# sqlite

SQLITE_VERSION := 3.6.20
SQLITE_URL := http://www.sqlite.org/sqlite-amalgamation-$(SQLITE_VERSION).tar.gz

# PKGS += sqlite

ifeq ($(call need_pkg,"sqlite3"),)
PKGS_FOUND += sqlite
endif

$(TARBALLS)/sqlite-$(SQLITE_VERSION).tar.gz:
	$(call download,$(SQLITE_URL))

.sum-sqlite: sqlite-$(SQLITE_VERSION).tar.gz

sqlite: sqlite-$(SQLITE_VERSION).tar.gz .sum-sqlite
	$(UNPACK)
	$(MOVE)

.sqlite: sqlite
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) && $(MAKE) install
	touch $@
