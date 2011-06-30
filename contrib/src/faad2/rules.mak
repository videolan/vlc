# faad2

FAAD2_VERSION := 2.7
FAAD2_URL := $(SF)/faac/faad2-$(FAAD2_VERSION).tar.gz

PKGS += faad2

$(TARBALLS)/faad2-$(FAAD2_VERSION).tar.gz:
	$(call download,$(FAAD2_URL))

.sum-faad2: faad2-$(FAAD2_VERSION).tar.gz

faad2: faad2-$(FAAD2_VERSION).tar.gz .sum-faad2
	$(UNPACK)
ifndef HAVE_FPU
	(cd $@-$(FAAD2_VERSION) && patch -p1) < $(SRC)/faad2/faad2-fixed.patch
endif
	(cd $@-$(FAAD2_VERSION) && $(CC) -iquote . -E - </dev/null || sed -i 's/-iquote /-I/' libfaad/Makefile.am)
	mv $@-$(FAAD2_VERSION) $@
	touch $@

.faad2: faad2
	cd $< && autoreconf -fiv
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) CFLAGS="$(NOTHUMB)"
	cd $< && sed -i.orig "s/shrext_cmds/shrext/g" libtool
	cd $</libfaad && $(MAKE) install
	touch $@
