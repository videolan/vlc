# sidplay2

SID_VERSION := 2.1.1
SID_URL := $(SF)/sidplay2/sidplay2/sidplay-libs-$(SID_VERSION)/sidplay-libs-$(SID_VERSION).tar.gz

ifdef GPL
PKGS += sidplay2
endif

ifeq ($(call need_pkg,"libsidplay2"),)
PKGS_FOUND += sidplay2
endif

$(TARBALLS)/sidplay-libs-$(SID_VERSION).tar.gz:
	$(call download_pkg,$(SID_URL),sidplay2)

.sum-sidplay2: sidplay-libs-$(SID_VERSION).tar.gz

sidplay-libs: sidplay-libs-$(SID_VERSION).tar.gz .sum-sidplay2
	$(UNPACK)
	$(APPLY) $(SRC)/sidplay2/sidplay2-openmode.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-endian.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-smartprt.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-noutils.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-string.patch
	$(APPLY) $(SRC)/sidplay2/sidplay-fix-ln-s.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-resid-dependency.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-char-cast.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-fix-overflow.patch
	$(MOVE)

.sidplay2: sidplay-libs
	$(REQUIRE_GPL)
	for d in . libsidplay builders resid builders/resid-builder \
			builders/hardsid-builder libsidutils ; \
	do \
		(cd $</$$d && rm -rf aclocal.m4 Makefile.in configure) || exit $$? ; \
	done
	for d in . libsidplay resid builders/resid-builder \
			builders/hardsid-builder libsidutils ; \
	do \
		(cd $</$$d && $(AUTORECONF) -fiv -I unix $(ACLOCAL_AMFLAGS)) || exit $$? ; \
	done
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	cp -- $(PREFIX)/lib/sidplay/builders/* "$(PREFIX)/lib/"
	touch $@
