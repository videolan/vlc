# ssh2

LIBSSH2_VERSION := 1.4.3
LIBSSH2_URL := http://www.libssh2.org/download/libssh2-$(LIBSSH2_VERSION).tar.gz

ifdef BUILD_NETWORK
PKGS += ssh2
endif
ifeq ($(call need_pkg,"libssh2"),)
PKGS_FOUND += ssh2
endif

ifeq ($(shell echo `${CC} -dumpversion | cut -f1-2 -d.`),4.9)
	BROKEN_GCC_CFLAGS:="CFLAGS=-O1"
endif

$(TARBALLS)/libssh2-$(LIBSSH2_VERSION).tar.gz:
	$(call download,$(LIBSSH2_URL))

.sum-ssh2: libssh2-$(LIBSSH2_VERSION).tar.gz

ssh2: libssh2-$(LIBSSH2_VERSION).tar.gz .sum-ssh2
	$(UNPACK)
	$(APPLY) $(SRC)/ssh2/no-tests.patch
	$(APPLY) $(SRC)/ssh2/configure-zlib.patch
	$(APPLY) $(SRC)/ssh2/gpg-error-pc.patch
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/ssh2/winrt-no-agent.patch
endif
	$(MOVE)

DEPS_ssh2 = gcrypt $(DEPS_gcrypt)

.ssh2: ssh2
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(BROKEN_GCC_CFLAGS) $(HOSTCONF) --disable-examples-build --with-libgcrypt --without-openssl
	cd $< && $(MAKE) install
	touch $@
