# NFS
NFS_VERSION := 6.0.2
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
	$(APPLY) $(SRC)/nfs/0001-cant-have-win32.h-referenced-from-a-header-we-instal.patch
	$(APPLY) $(SRC)/nfs/0002-pthread-and-win32-need-to-be-exclusive-in-multithrea.patch
	$(APPLY) $(SRC)/nfs/0003-win32-define-struct-timezone-for-non-mingw-w32.patch
	$(APPLY) $(SRC)/nfs/0004-win32-fix-build-with-MSVC.patch
	$(APPLY) $(SRC)/nfs/0005-win32-don-t-use-pthread-on-Windows.patch
	$(APPLY) $(SRC)/nfs/0001-cmake-export-the-necessary-library-in-the-pkg-config.patch
	$(MOVE)

.nfs: nfs toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
