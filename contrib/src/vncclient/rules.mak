# vncclient

VNCCLIENT_VERSION := 0.9.10
VNCCLIENT_URL := https://github.com/LibVNC/libvncserver/archive/LibVNCServer-$(VNCCLIENT_VERSION).tar.gz

ifdef BUILD_NETWORK
PKGS += vncclient
endif
ifeq ($(call need_pkg,"libvncclient"),)
PKGS_FOUND += vncclient
endif

$(TARBALLS)/LibVNCServer-$(VNCCLIENT_VERSION).tar.gz:
	$(call download_pkg,$(VNCCLIENT_URL),vncclient)

.sum-vncclient: LibVNCServer-$(VNCCLIENT_VERSION).tar.gz

vncclient: LibVNCServer-$(VNCCLIENT_VERSION).tar.gz .sum-vncclient
	$(UNPACK)
	mv libvncserver-LibVNCServer-$(VNCCLIENT_VERSION)  LibVNCServer-$(VNCCLIENT_VERSION)
	$(APPLY) $(SRC)/vncclient/libvncclient-libjpeg-win32.patch
	$(APPLY) $(SRC)/vncclient/rfbproto.patch
	$(APPLY) $(SRC)/vncclient/png-detection.patch
	$(APPLY) $(SRC)/vncclient/vnc-gnutls-pkg.patch
	$(APPLY) $(SRC)/vncclient/gnutls-recent.patch
	$(APPLY) $(SRC)/vncclient/vnc-gnutls-anon.patch
	$(APPLY) $(SRC)/vncclient/cross-ar.patch
	$(call pkg_static,"libvncclient.pc.in")
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

DEPS_vncclient = gcrypt $(DEPS_gcrypt) jpeg $(DEPS_jpeg) png $(DEPS_png) gnutls $(DEPS_gnutls)

VNCCLIENT_CONF := $(HOSTCONF) --without-libva
ifdef HAVE_WIN32
VNCCLIENT_CONF += --without-pthread
endif

.vncclient: vncclient
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(VNCCLIENT_CONF)
	cd $< && $(MAKE) -C libvncclient install
	cd $< && $(MAKE) install-data
	rm $(PREFIX)/lib/pkgconfig/libvncserver.pc
	touch $@
