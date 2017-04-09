# ssh2

LIBSSH2_VERSION := 1.8.0
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
	$(call download_pkg,$(LIBSSH2_URL),ssh2)

.sum-ssh2: libssh2-$(LIBSSH2_VERSION).tar.gz

ssh2: libssh2-$(LIBSSH2_VERSION).tar.gz .sum-ssh2
	$(UNPACK)
	$(APPLY) $(SRC)/ssh2/no-tests.patch
	$(APPLY) $(SRC)/ssh2/ced924b78a40126606797ef57a74066eb3b4b83f.patch
	$(APPLY) $(SRC)/ssh2/0001-Add-lgpg-error-to-.pc-to-facilitate-static-linking.patch
	$(call pkg_static,"libssh2.pc.in")
ifdef HAVE_WINSTORE
	$(APPLY) $(SRC)/ssh2/winrt-no-agent.patch
endif
	$(MOVE)

DEPS_ssh2 = gcrypt $(DEPS_gcrypt)

.ssh2: ssh2
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(BROKEN_GCC_CFLAGS) $(HOSTCONF) --disable-examples-build --with-libgcrypt --without-openssl --without-mbedtls
	cd $< && $(MAKE) install
	touch $@
