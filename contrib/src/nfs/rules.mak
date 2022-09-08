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

nfs: libnfs-$(NFS_VERSION).tar.gz .sum-nfs
	$(UNPACK)
	mv libnfs-libnfs-$(NFS_VERSION) libnfs-$(NFS_VERSION)
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

NFS_CONF := --disable-examples --disable-utils --disable-werror

.nfs: nfs
	cd $< && ./bootstrap
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(NFS_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
