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

.sum-mpcdec: musepack_src_r$(MUSE_REV).tar.gz

musepack: musepack_src_r$(MUSE_REV).tar.gz .sum-mpcdec
	$(UNPACK)
	$(APPLY) $(SRC)/mpcdec/musepack-no-cflags-clobber.patch
	$(APPLY) $(SRC)/mpcdec/musepack-no-binaries.patch
	$(APPLY) $(SRC)/mpcdec/musepack-asinh-msvc.patch
	$(APPLY) $(SRC)/mpcdec/0004-libmpcdec-added-install-and-soversion.patch
	$(APPLY) $(SRC)/mpcdec/0005-If-BUILD_SHARED_LIBS-is-set-and-SHARED-undefined-the.patch
	$(APPLY) $(SRC)/mpcdec/0006-adapted-patch-0001-shared.patch-from-buildroot.patch
	$(MOVE)

.mpcdec: musepack toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(MUSE_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
