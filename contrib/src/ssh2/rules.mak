# ssh2

LIBSSH2_VERSION := 1.4.3
LIBSSH2_URL := http://www.libssh2.org/download/libssh2-$(LIBSSH2_VERSION).tar.gz

ifdef BUILD_NETWORK
PKGS += ssh2
endif
ifeq ($(call need_pkg,"libssh2"),)
PKGS_FOUND += ssh2
endif

$(TARBALLS)/libssh2-$(LIBSSH2_VERSION).tar.gz:
	$(call download,$(LIBSSH2_URL))

.sum-ssh2: libssh2-$(LIBSSH2_VERSION).tar.gz

ssh2: libssh2-$(LIBSSH2_VERSION).tar.gz .sum-ssh2
	$(UNPACK)
	$(APPLY) $(SRC)/ssh2/no-tests.patch
	$(MOVE)

DEPS_ssh2 = gcrypt $(DEPS_gcrypt)

.ssh2: ssh2
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-examples-build
	cd $< && $(MAKE) install
	touch $@
