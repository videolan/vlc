# vncclient

VNCCLIENT_VERSION := 0.9.15
VNCCLIENT_URL := $(GITHUB)/LibVNC/libvncserver/archive/LibVNCServer-$(VNCCLIENT_VERSION).tar.gz

ifdef GPL
ifdef BUILD_NETWORK
PKGS += vncclient
endif
ifeq ($(call need_pkg,"libvncclient"),)
PKGS_FOUND += vncclient
endif
endif

$(TARBALLS)/LibVNCServer-$(VNCCLIENT_VERSION).tar.gz:
	$(call download_pkg,$(VNCCLIENT_URL),vncclient)

.sum-vncclient: LibVNCServer-$(VNCCLIENT_VERSION).tar.gz

vncclient: LibVNCServer-$(VNCCLIENT_VERSION).tar.gz .sum-vncclient
	$(UNPACK)
	mv libvncserver-LibVNCServer-$(VNCCLIENT_VERSION)  LibVNCServer-$(VNCCLIENT_VERSION)
	$(APPLY) $(SRC)/vncclient/vnc-gnutls-anon.patch
	$(APPLY) $(SRC)/vncclient/build-add-install-components-for-client-server.patch
	$(APPLY) $(SRC)/vncclient/CMake-use-Requires-for-.pc-generation-don-t-parse-li.patch
	$(APPLY) $(SRC)/vncclient/0001-libvncclient-tls_gnutls-Fix-gnutls_transport_set_err.patch
	$(APPLY) $(SRC)/vncclient/0002-CMake-Fix-the-include-guards-in-rfbconfig.h.patch
	$(call pkg_static,"src/libvncclient/libvncclient.pc.cmakein")
	$(MOVE)

DEPS_vncclient = gcrypt $(DEPS_gcrypt) jpeg $(DEPS_jpeg) png $(DEPS_png) gnutls $(DEPS_gnutls)

VNCCLIENT_CONF := \
	-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
	-DWITH_OPENSSL:BOOL=OFF \
	-DWITH_GCRYPT:BOOL=ON \
	-DWITH_ZLIB:BOOL=ON \
	-DWITH_JPEG:BOOL=ON \
	-DWITH_PNG:BOOL=ON \
	-DWITH_SDL:BOOL=OFF \
	-DWITH_GTK:BOOL=OFF \
	-DWITH_LIBSSHTUNNEL:BOOL=OFF \
	-DWITH_THREADS:BOOL=ON \
	-DWITH_SYSTEMD:BOOL=OFF \
	-DWITH_FFMPEG:BOOL=OFF \
	-DWITH_WEBSOCKETS:BOOL=OFF \
	-DWITH_EXAMPLES:BOOL=OFF \
	-DWITH_TESTS:BOOL=OFF

ifdef HAVE_WIN32
VNCCLIENT_CONF += -DPREFER_WIN32THREADS:BOOL=ON
endif

.vncclient: vncclient toolchain.cmake
	$(REQUIRE_GPL)
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(VNCCLIENT_CONF)
	+$(CMAKEBUILD) --target vncclient
	$(CMAKEINSTALL) --component vncclient
	touch $@
