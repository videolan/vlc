# ssh2

LIBSSH2_VERSION := 1.11.1
LIBSSH2_URL := https://www.libssh2.org/download/libssh2-$(LIBSSH2_VERSION).tar.xz

ifdef BUILD_NETWORK
PKGS += ssh2
endif
ifeq ($(call need_pkg,"libssh2"),)
PKGS_FOUND += ssh2
endif

$(TARBALLS)/libssh2-$(LIBSSH2_VERSION).tar.xz:
	$(call download_pkg,$(LIBSSH2_URL),ssh2)

.sum-ssh2: libssh2-$(LIBSSH2_VERSION).tar.xz

ssh2: libssh2-$(LIBSSH2_VERSION).tar.xz .sum-ssh2
	$(UNPACK)
	$(MOVE)

DEPS_ssh2 =
ifndef HAVE_WIN32
DEPS_ssh2 += gcrypt $(DEPS_gcrypt)
else
ifdef HAVE_WINSTORE
# uses SecureZeroMemory
DEPS_ssh2 += alloweduwp $(DEPS_alloweduwp)
endif
endif

SSH2_CONF := -DBUILD_EXAMPLES=OFF -DLIBSSH2_BUILD_DOCS=OFF
ifndef HAVE_WIN32
SSH2_CONF += -DCRYPTO_BACKEND:STRING=Libgcrypt
else
SSH2_CONF += -DCRYPTO_BACKEND:STRING=WinCNG
endif

.ssh2: ssh2 toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(SSH2_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
