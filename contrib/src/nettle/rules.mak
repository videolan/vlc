# Nettle

NETTLE_VERSION := 3.7.3
NETTLE_URL := $(GNU)/nettle/nettle-$(NETTLE_VERSION).tar.gz

ifeq ($(call need_pkg,"nettle >= 2.7"),)
PKGS_FOUND += nettle
endif

ifdef HAVE_WIN32
ifeq ($(ARCH),arm)
NETTLE_CONF += --disable-assembler
endif
endif

$(TARBALLS)/nettle-$(NETTLE_VERSION).tar.gz:
	$(call download_pkg,$(NETTLE_URL),nettle)

.sum-nettle: nettle-$(NETTLE_VERSION).tar.gz

nettle: nettle-$(NETTLE_VERSION).tar.gz .sum-nettle
	$(UNPACK)
	$(MOVE)

DEPS_nettle = gmp $(DEPS_gmp)

# GMP requires either GPLv2 or LGPLv3
.nettle: nettle
ifndef GPL
	$(REQUIRE_GNUV3)
endif
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(NETTLE_CONF)
	cd $< && $(MAKE) install
	touch $@
