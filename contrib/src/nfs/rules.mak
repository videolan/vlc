# NFS
NFS_VERSION := 1.9.8
NFS_URL := https://github.com/sahlberg/libnfs/archive/libnfs-$(NFS_VERSION).tar.gz

ifndef HAVE_WIN32
PKGS += nfs
endif
ifeq ($(call need_pkg,"libnfs"),)
PKGS_FOUND += nfs
endif

$(TARBALLS)/libnfs-$(NFS_VERSION).tar.gz:
	$(call download,$(NFS_URL))

.sum-nfs: libnfs-$(NFS_VERSION).tar.gz

nfs: libnfs-$(NFS_VERSION).tar.gz .sum-nfs
	$(UNPACK)
	mv libnfs-libnfs-$(NFS_VERSION) libnfs-$(NFS_VERSION)
	$(MOVE)

.nfs: nfs
	cd $< && ./bootstrap
	cd $< && $(HOSTVARS) ./configure --disable-examples $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
