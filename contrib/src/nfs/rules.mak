# NFS
NFS_VERSION := 8.0.1
NFS_URL := $(GITHUB)/sahlberg/libnfs/archive/libnfs-$(NFS_VERSION).tar.gz

ifdef BUILD_NETWORK
PKGS += nfs
ifeq ($(call need_pkg,"libnfs >= 1.10"),)
PKGS_FOUND += nfs
endif
endif

ifneq ($(findstring gnutls,$(PKGS)),)
DEPS_nfs = gnutls $(DEPS_gnutls)
endif

$(TARBALLS)/libnfs-$(NFS_VERSION).tar.gz:
	$(call download_pkg,$(NFS_URL),nfs)

.sum-nfs: libnfs-$(NFS_VERSION).tar.gz

nfs: UNPACK_DIR=libnfs-libnfs-$(NFS_VERSION)
nfs: libnfs-$(NFS_VERSION).tar.gz .sum-nfs
	$(UNPACK)
	$(APPLY) $(SRC)/nfs/0001-cmake-use-Requires.private-for-gnutls-in-the-pkg-con.patch
	$(call pkg_static,"cmake/libnfs.pc.cmake")
	$(MOVE)

.nfs: nfs toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
