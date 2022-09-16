# libtasn1

LIBTASN1_VERSION := 4.19.0
LIBTASN1_URL := $(GNU)/libtasn1/libtasn1-$(LIBTASN1_VERSION).tar.gz

ifeq ($(call need_pkg,"libtasn1 >= 4.3"),)
PKGS_FOUND += libtasn1
endif

$(TARBALLS)/libtasn1-$(LIBTASN1_VERSION).tar.gz:
	$(call download_pkg,$(LIBTASN1_URL),libtasn1)

.sum-libtasn1: libtasn1-$(LIBTASN1_VERSION).tar.gz

libtasn1: libtasn1-$(LIBTASN1_VERSION).tar.gz .sum-libtasn1
	$(UNPACK)
	$(APPLY) $(SRC)/libtasn1/no-executables.patch
	$(APPLY) $(SRC)/libtasn1/0001-fcntl-do-not-call-GetHandleInformation-in-Winstore-a.patch
	# on iOS for some reason _GNU_SOURCE is found in config.h but strverscmp() is not found
	cd $(UNPACK_DIR) && sed -i.orig -e 's, -DASN1_BUILDING, -DASN1_BUILDING -D_GNU_SOURCE,' lib/Makefile.am
	$(MOVE)

LIBTASN1_CONF := --disable-doc

.libtasn1: libtasn1
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) $(LIBTASN1_CONF)
	cd $< && $(MAKE) install
	touch $@
