# Musepack decoder

PKGS += mpcdec

#MUSE_VERSION := 1.2.6
#MUSE_URL := http://files.musepack.net/source/libmpcdec-$(MUSE_VERSION).tar.bz2
#MUSE_URL := http://files2.musepack.net/source/libmpcdec-$(MUSE_VERSION).tar.bz2

#MUSE_REV := 475
#MUSE_URL := http://files.musepack.net/source/musepack_src_r$(MUSE_REV).tar.gz

#$(TARBALLS)/musepack_src_r$(MUSE_REV).tar.gz:
#	$(call download,$(MUSE_URL))

MUSE_REV := 481
MUSE_SVN := http://svn.musepack.net/libmpc/trunk/
#
$(TARBALLS)/musepack_src_r$(MUSE_REV).tar.gz:
	rm -Rf musepack_src_r$(MUSE_REV)
	$(SVN) export $(MUSE_SVN) -r $(MUSE_REV) musepack_src_r$(MUSE_REV)
	tar czv musepack_src_r$(MUSE_REV) > $@

.sum-mpcdec: musepack_src_r$(MUSE_REV).tar.gz
	$(warning $@ not implemented)
	touch $@

musepack: musepack_src_r$(MUSE_REV).tar.gz .sum-mpcdec
	$(UNPACK)
	$(APPLY) $(SRC)/mpcdec/musepack-no-cflags-clobber.patch
	$(APPLY) $(SRC)/mpcdec/musepack-no-binaries.patch
	sed -i.orig \
		-e 's,^add_subdirectory(mpcgain),,g' \
		-e 's,^add_subdirectory(mpcchap),,g' \
		$@_src_r$(MUSE_REV)/CMakeLists.txt
ifdef HAVE_MACOSX
	cd musepack_src_r$(MUSE_REV) && \
	sed -e 's%-O3 -Wall%-O3 -Wall $(CFLAGS)%' CMakeLists.txt
endif
	mv $@_src_r$(MUSE_REV) $@
	touch $@

.mpcdec: musepack toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) -DSHARED=OFF .
	cd $< && $(MAKE) install
	mkdir -p -- "$(PREFIX)/lib"
	cd $< && cp libmpcdec/libmpcdec_static.a "$(PREFIX)/lib/libmpcdec.a"
	touch $@
