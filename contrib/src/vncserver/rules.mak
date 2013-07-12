# vncserver

VNCSERVER_VERSION := 0.9.9
VNCSERVER_URL := $(SF)/libvncserver/libvncserver/$(VNCSERVER_VERSION)/LibVNCServer-$(VNCSERVER_VERSION).tar.gz

PKGS += vncserver
ifeq ($(call need_pkg,"libvncclient"),)
PKGS_FOUND += vncserver
endif

$(TARBALLS)/LibVNCServer-$(VNCSERVER_VERSION).tar.gz:
	$(call download,$(VNCSERVER_URL))

.sum-vncserver: LibVNCServer-$(VNCSERVER_VERSION).tar.gz

vncserver: LibVNCServer-$(VNCSERVER_VERSION).tar.gz .sum-vncserver
	$(UNPACK)
	$(APPLY) $(SRC)/vncserver/libvncclient-libjpeg-win32.patch
	$(APPLY) $(SRC)/vncserver/rfbproto.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

DEPS_vncserver = gcrypt $(DEPS_gcrypt) jpeg $(DEPS_jpeg) png $(DEPS_png)

.vncserver: vncserver
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) -C libvncclient install
	touch $@
