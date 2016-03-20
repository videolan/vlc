# vncserver

VNCSERVER_VERSION := 0.9.10
VNCSERVER_URL := https://github.com/LibVNC/libvncserver/archive/LibVNCServer-$(VNCSERVER_VERSION).tar.gz

ifdef BUILD_NETWORK
PKGS += vncserver
endif
ifeq ($(call need_pkg,"libvncclient"),)
PKGS_FOUND += vncserver
endif

$(TARBALLS)/LibVNCServer-$(VNCSERVER_VERSION).tar.gz:
	$(call download,$(VNCSERVER_URL))

.sum-vncserver: LibVNCServer-$(VNCSERVER_VERSION).tar.gz

vncserver: LibVNCServer-$(VNCSERVER_VERSION).tar.gz .sum-vncserver
	$(UNPACK)
	mv libvncserver-LibVNCServer-$(VNCSERVER_VERSION)  LibVNCServer-$(VNCSERVER_VERSION)
	$(APPLY) $(SRC)/vncserver/libvncclient-libjpeg-win32.patch
	$(APPLY) $(SRC)/vncserver/rfbproto.patch
	$(APPLY) $(SRC)/vncserver/png-detection.patch
	$(APPLY) $(SRC)/vncserver/vnc-gnutls-pkg.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

DEPS_vncserver = gcrypt $(DEPS_gcrypt) jpeg $(DEPS_jpeg) png $(DEPS_png) gnutls $(DEP_gnutls)

.vncserver: vncserver
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) -C libvncclient install
	cd $< && $(MAKE) install-data
	touch $@
