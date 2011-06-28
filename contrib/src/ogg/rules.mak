# libogg

OGG_VERSION := 1.2.2

OGG_URL := http://downloads.xiph.org/releases/ogg/libogg-$(OGG_VERSION).tar.xz
#OGG_URL := $(CONTRIB_VIDEOLAN)/libogg-$(OGG_VERSION).tar.xz
OGG_CVSROOT := :pserver:anoncvs@xiph.org:/usr/local/cvsroot

NEED_OGG = $(call need_pkg,"ogg >= 1.0")

$(TARBALLS)/libogg-$(OGG_VERSION).tar.xz:
	$(DOWNLOAD) $(OGG_URL)

.sum-ogg: libogg-$(OGG_VERSION).tar.xz

libogg: libogg-$(OGG_VERSION).tar.xz .sum-ogg
	$(UNPACK)
	(cd $@-$(OGG_VERSION) && patch -p1) < $(SRC)/ogg/libogg-1.1.patch
ifdef HAVE_WINCE
	(cd $@-$(OGG_VERSION) && patch -p1) < $(SRC)/ogg/libogg-wince.patch
endif
	mv $@-$(OGG_VERSION) $@
	touch $@

ifeq ($(NEED_OGG),)
.ogg:
else
PKGS += ogg

.ogg: libogg
	#cd $< && autoreconf
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
endif
	touch $@
