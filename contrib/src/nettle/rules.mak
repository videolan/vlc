# Nettle

NETTLE_VERSION := 2.2
NETTLE_URL := ftp://ftp.gnu.org/gnu/nettle/nettle-$(NETTLE_VERSION).tar.gz

# PKGS += nettle

$(TARBALLS)/nettle-$(NETTLE_VERSION).tar.gz:
	$(call download,$(NETTLE_URL))

.sum-nettle: nettle-$(NETTLE_VERSION).tar.gz

nettle: nettle-$(NETTLE_VERSION).tar.gz .sum-nettle
	$(UNPACK)
	$(MOVE)

.nettle: nettle
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
