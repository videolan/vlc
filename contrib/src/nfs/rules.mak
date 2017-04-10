# NFS
NFS_VERSION := 1.11.0
NFS_URL := https://github.com/sahlberg/libnfs/archive/libnfs-$(NFS_VERSION).tar.gz

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
	$(APPLY) $(SRC)/nfs/win32.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

.nfs: nfs
	cd $< && ./bootstrap
	cd $< && $(HOSTVARS) ./configure --disable-examples --disable-utils --disable-werror $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
