# CDDB
CDDB_VERSION := 1.3.2
CDDB_URL := $(SF)/libcddb/libcddb-$(CDDB_VERSION).tar.bz2

ifdef BUILD_DISCS
PKGS += cddb
endif
ifeq ($(call need_pkg,"libcddb"),)
PKGS_FOUND += cddb
endif

$(TARBALLS)/libcddb-$(CDDB_VERSION).tar.bz2:
	$(call download,$(CDDB_URL))

.sum-cddb: libcddb-$(CDDB_VERSION).tar.bz2

cddb: libcddb-$(CDDB_VERSION).tar.bz2 .sum-cddb
	$(UNPACK)
	$(APPLY) $(SRC)/cddb/cross.patch
	$(APPLY) $(SRC)/cddb/getenv-crash.patch
	$(APPLY) $(SRC)/cddb/cddb-no-alarm.patch
ifdef HAVE_WIN32
	$(APPLY) $(SRC)/cddb/win32-pkg.patch
endif
	$(MOVE)

DEPS_cddb = regex $(DEPS_regex)

.cddb: cddb
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --without-iconv CFLAGS="$(CFLAGS) -D_BSD_SOCKLEN_T_=int -DWIN32_LEAN_AND_MEAN"
	cd $< && $(MAKE) install
	touch $@
