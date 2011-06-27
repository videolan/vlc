# libogg

OGG_VERSION := 1.2.2

OGG_TARBALL := libogg-$(OGG_VERSION).tar.xz
OGG_URL := http://downloads.xiph.org/releases/ogg/$(OGG_TARBALL)
#OGG_URL := $(CONTRIB_VIDEOLAN)/$(OGG_TARBALL)
OGG_CVSROOT := :pserver:anoncvs@xiph.org:/usr/local/cvsroot

NEED_OGG = $(call need_pkg,"ogg >= 1.0")

$(TARBALLS)/$(OGG_TARBALL):
	$(DOWNLOAD) $(OGG_URL)

.sum-ogg: $(TARBALLS)/$(OGG_TARBALL)
	$(CHECK_SHA512)
	touch $@

libogg: $(TARBALLS)/$(OGG_TARBALL) .sum-ogg
	$(UNPACK_XZ)
	(cd $@-$(OGG_VERSION) && patch -p1) < $(SRC)/ogg/libogg-1.1.patch
ifdef HAVE_WINCE
	(cd $@-$(OGG_VERSION) && patch -p1) < $(SRC)/ogg/libogg-wince.patch
endif
	mv $@-$(OGG_VERSION) $@

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
