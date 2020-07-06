# Musepack decoder

PKGS += mpcdec

#MUSE_VERSION := 1.2.6
#MUSE_URL := http://files.musepack.net/source/libmpcdec-$(MUSE_VERSION).tar.bz2
#MUSE_URL := http://files2.musepack.net/source/libmpcdec-$(MUSE_VERSION).tar.bz2

MUSE_REV := 481
MUSE_URL := $(CONTRIB_VIDEOLAN)/mpcdec/musepack_src_r$(MUSE_REV).tar.gz

$(TARBALLS)/musepack_src_r$(MUSE_REV).tar.gz:
	$(call download_pkg,$(MUSE_URL),mpcdec)

#MUSE_REV := 481
#MUSE_SVN := http://svn.musepack.net/libmpc/trunk/

#$(TARBALLS)/musepack_src_r$(MUSE_REV).tar.gz:
#	rm -Rf musepack_src_r$(MUSE_REV)
#	$(SVN) export $(MUSE_SVN) -r $(MUSE_REV) musepack_src_r$(MUSE_REV)
#	tar czv musepack_src_r$(MUSE_REV) > $@

.sum-mpcdec: musepack_src_r$(MUSE_REV).tar.gz
#	$(warning $@ not implemented)
#	touch $@

musepack: musepack_src_r$(MUSE_REV).tar.gz .sum-mpcdec
	$(UNPACK)
	$(APPLY) $(SRC)/mpcdec/musepack-no-cflags-clobber.patch
	$(APPLY) $(SRC)/mpcdec/musepack-no-binaries.patch
ifdef HAVE_VISUALSTUDIO
	$(APPLY) $(SRC)/mpcdec/musepack-asinh-msvc.patch
endif
	sed -i.orig \
		-e 's,^add_subdirectory(mpcgain),,g' \
		-e 's,^add_subdirectory(mpcchap),,g' \
		$(UNPACK_DIR)/CMakeLists.txt
ifdef HAVE_MACOSX
	cd $(UNPACK_DIR) && \
	sed -e 's%-O3 -Wall%-O3 -Wall $(CFLAGS)%' CMakeLists.txt
endif
	$(MOVE)

.mpcdec: musepack toolchain.cmake
	cd $< && $(HOSTVARS_PIC) $(CMAKE) -DSHARED=OFF .
	cd $< && $(MAKE) install
	mkdir -p -- "$(PREFIX)/lib"
	# Use globbing to work around cmake's change of destination file
	cd $< && cp libmpcdec/*mpcdec_static.* "$(PREFIX)/lib/libmpcdec.a"
	touch $@
