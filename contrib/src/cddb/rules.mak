# CDDB
CDDB_VERSION := 1.3.2
CDDB_URL := $(SF)/libcddb/libcddb-$(CDDB_VERSION).tar.bz2

ifdef BUILD_NETWORK
ifdef BUILD_DISCS
PKGS += cddb
endif
endif
ifeq ($(call need_pkg,"libcddb"),)
PKGS_FOUND += cddb
endif

$(TARBALLS)/libcddb-$(CDDB_VERSION).tar.bz2:
	$(call download_pkg,$(CDDB_URL),cddb)

.sum-cddb: libcddb-$(CDDB_VERSION).tar.bz2

cddb: libcddb-$(CDDB_VERSION).tar.bz2 .sum-cddb
	$(UNPACK)
	# $(call update_autoconfig,.)
	$(APPLY) $(SRC)/cddb/cross.patch
	$(APPLY) $(SRC)/cddb/getenv-crash.patch
	$(APPLY) $(SRC)/cddb/cddb-no-alarm.patch
	$(APPLY) $(SRC)/cddb/fix-header-guards.patch
	$(APPLY) $(SRC)/cddb/no-gettext.patch
	$(APPLY) $(SRC)/cddb/cddb-gcc14-getsockoptfix.patch
	# Avoid relying on iconv.m4 from gettext, when reconfiguring.
	# This is only used by the frontend which we disable.
	sed -i.orig 's/^[[:blank:]]*AM_ICONV/#&/' $(UNPACK_DIR)/configure.ac
	# add internal dependencies
	sed -i.orig 's/-lcddb @LIBICONV@/-lcddb @LIBS@/' $(UNPACK_DIR)/libcddb.pc.in
	$(MOVE)

DEPS_cddb = regex $(DEPS_regex)

CDDB_CONF := --without-iconv

CDDB_CFLAGS := $(CFLAGS) -D_BSD_SOCKLEN_T_=int
ifdef HAVE_WIN32
CDDB_CFLAGS += -DWIN32_LEAN_AND_MEAN
endif
CDDB_CONF += CFLAGS="$(CDDB_CFLAGS)"

.cddb: cddb
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(CDDB_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
