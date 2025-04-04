# ssh2

LIBSSH2_VERSION := 1.11.0
LIBSSH2_URL := https://www.libssh2.org/download/libssh2-$(LIBSSH2_VERSION).tar.gz

ifdef BUILD_NETWORK
PKGS += ssh2
endif
ifeq ($(call need_pkg,"libssh2"),)
PKGS_FOUND += ssh2
endif

$(TARBALLS)/libssh2-$(LIBSSH2_VERSION).tar.gz:
	$(call download_pkg,$(LIBSSH2_URL),ssh2)

.sum-ssh2: libssh2-$(LIBSSH2_VERSION).tar.gz

ssh2: libssh2-$(LIBSSH2_VERSION).tar.gz .sum-ssh2
	$(UNPACK)
	$(call update_autoconfig,.)
	$(call pkg_static,"libssh2.pc.in")
	$(MOVE)

DEPS_ssh2 = gcrypt $(DEPS_gcrypt)
ifdef HAVE_WINSTORE
# uses SecureZeroMemory
DEPS_ssh2 += alloweduwp $(DEPS_alloweduwp)
endif

SSH2_CONF := --disable-examples-build --disable-tests --with-crypto=libgcrypt

.ssh2: ssh2
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(SSH2_CONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
