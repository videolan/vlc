# asdcplib

ASDCPLIB_VERSION := 2.7.19

ASDCPLIB_URL := http://download.cinecert.com/asdcplib/asdcplib-$(ASDCPLIB_VERSION).tar.gz

ifndef HAVE_IOS
ifndef HAVE_ANDROID
ifndef HAVE_WINSTORE
PKGS += asdcplib
endif
endif
endif

ifeq ($(call need_pkg,"asdcplib >= 1.12"),)
PKGS_FOUND += asdcplib
endif

$(TARBALLS)/asdcplib-$(ASDCPLIB_VERSION).tar.gz:
	$(call download_pkg,$(ASDCPLIB_URL),asdcplib)

.sum-asdcplib: asdcplib-$(ASDCPLIB_VERSION).tar.gz

asdcplib: asdcplib-$(ASDCPLIB_VERSION).tar.gz .sum-asdcplib
	$(UNPACK)
	$(APPLY) $(SRC)/asdcplib/port-to-nettle.patch
	$(APPLY) $(SRC)/asdcplib/static-programs.patch
	$(APPLY) $(SRC)/asdcplib/adding-pkg-config-file.patch
	$(APPLY) $(SRC)/asdcplib/win32-cross-compilation.patch
	$(APPLY) $(SRC)/asdcplib/win32-dirent.patch
	$(MOVE)

DEPS_asdcplib = nettle $(DEPS_nettle)

.asdcplib: asdcplib
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --enable-freedist --enable-dev-headers --with-nettle=$(PREFIX)
	cd $< && $(MAKE) install
	mkdir -p -- "$(PREFIX)/lib/pkgconfig"
	cp $</asdcplib.pc "$(PREFIX)/lib/pkgconfig/"
	touch $@
