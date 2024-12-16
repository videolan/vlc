# NFS
NFS_VERSION := 5.0.2
NFS_URL := $(GITHUB)/sahlberg/libnfs/archive/libnfs-$(NFS_VERSION).tar.gz

PKGS += nfs
ifeq ($(call need_pkg,"libnfs >= 1.10"),)
PKGS_FOUND += nfs
endif

$(TARBALLS)/libnfs-$(NFS_VERSION).tar.gz:
	$(call download_pkg,$(NFS_URL),nfs)

.sum-nfs: libnfs-$(NFS_VERSION).tar.gz

nfs: UNPACK_DIR=libnfs-libnfs-$(NFS_VERSION)
nfs: libnfs-$(NFS_VERSION).tar.gz .sum-nfs
	$(UNPACK)
	$(MOVE)

.nfs: nfs toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_PIC) $(CMAKE)
	+$(CMAKEBUILD)
	+$(CMAKEBUILD) --target install
	touch $@
