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
	$(call update_autoconfig,unix)
	$(call update_autoconfig,builders/resid/unix)
	$(call update_autoconfig,builders/resid-builder/unix)
	$(call update_autoconfig,builders/hardsid-builder/unix)
	$(call update_autoconfig,libsidplay/unix)
	$(call update_autoconfig,libsidutils/unix)
	$(APPLY) $(SRC)/sidplay2/sidplay2-openmode.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-endian.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-smartprt.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-noutils.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-string.patch
	$(APPLY) $(SRC)/sidplay2/sidplay-fix-ln-s.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-resid-dependency.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-char-cast.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-fix-overflow.patch
	$(APPLY) $(SRC)/sidplay2/sidplay2-cxxtest.patch
	# don't depend on libtool to use libsidplay2
	sed -i.orig 's,$${libdir}/libsidplay2.la,-L$${libdir} -lsidplay2,' "$(UNPACK_DIR)/libsidplay/unix/libsidplay2.pc.in"
	$(MOVE)

.sidplay2: sidplay-libs
	$(REQUIRE_GPL)
	for d in . libsidplay resid builders/resid-builder \
			builders/hardsid-builder libsidutils ; \
	do \
		(cd $</$$d && $(AUTORECONF) -fiv -I unix) || exit $$? ; \
	done
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	$(MAKE) -C $<
	$(MAKE) -C $< install
	cp -- $(PREFIX)/lib/sidplay/builders/* "$(PREFIX)/lib/"
	touch $@
