# ssh2

LIBSSH2_VERSION := 1.11.1
LIBSSH2_URL := http://www.libssh2.org/download/libssh2-$(LIBSSH2_VERSION).tar.xz

ifdef BUILD_NETWORK
PKGS += ssh2
endif
ifeq ($(call need_pkg,"libssh2"),)
PKGS_FOUND += ssh2
endif

ifeq ($(shell echo `${CC} -dumpversion | cut -f1-2 -d.`),4.9)
	BROKEN_GCC_CFLAGS:="CFLAGS=-O1"
endif

$(TARBALLS)/libssh2-$(LIBSSH2_VERSION).tar.xz:
	$(call download_pkg,$(LIBSSH2_URL),ssh2)

.sum-ssh2: libssh2-$(LIBSSH2_VERSION).tar.xz

ssh2: libssh2-$(LIBSSH2_VERSION).tar.xz .sum-ssh2
	$(UNPACK)
	$(MOVE)

DEPS_ssh2 = gcrypt $(DEPS_gcrypt)
ifdef HAVE_WINSTORE
# uses SecureZeroMemory
DEPS_ssh2 += alloweduwp $(DEPS_alloweduwp)
endif

SSH2_CONF := -DBUILD_EXAMPLES=OFF -DLIBSSH2_BUILD_DOCS=OFF -DCRYPTO_BACKEND:STRING=Libgcrypt $(BROKEN_GCC_CFLAGS)

.ssh2: ssh2 toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(SSH2_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
