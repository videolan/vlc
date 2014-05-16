# asdcplib

ASDCPLIB_VERSION := 1.12.58

ASDCPLIB_URL := http://download.cinecert.com/asdcplib/asdcplib-$(ASDCPLIB_VERSION).tar.gz

#PKGS += asdcplib
ifeq ($(call need_pkg,"asdcplib >= 1.12"),)
PKGS_FOUND += asdcplib
endif

$(TARBALLS)/asdcplib-$(ASDCPLIB_VERSION).tar.gz:
	$(call download,$(ASDCPLIB_URL))

.sum-asdcplib: asdcplib-$(ASDCPLIB_VERSION).tar.gz

asdcplib: asdcplib-$(ASDCPLIB_VERSION).tar.gz .sum-asdcplib
	$(UNPACK)
	$(APPLY) $(SRC)/asdcplib/win32-cross-compilation.patch
	$(MOVE)

.asdcplib: asdcplib
	$(RECONF)
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --enable-freedist
	cd $< && $(MAKE) install
	touch $@
