# faad2

FAAD2_VERSION := 2.7
FAAD2_URL := $(SF)/faac/faad2-$(FAAD2_VERSION).tar.gz

PKGS += faad2

$(TARBALLS)/faad2-$(FAAD2_VERSION).tar.gz:
	$(call download,$($(FAAD2_URL))

.sum-faad2: faad2-$(FAAD2_VERSION).tar.gz

faad2: faad2-$(FAAD2_VERSION).tar.gz .sum-faad2
	$(UNPACK)
	(cd $@-$(FAAD2_VERSION); echo|$(CC) -iquote . -E - || sed -i 's/-iquote /-I/' libfaad/Makefile.am; autoreconf -ivf)
	patch -p0 < $(SRC)/faad2/arm-fixed.patch
	mv $@-$(FAAD2_VERSION) $@
	touch $@

.faad2: faad2
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) CFLAGS="$(NOTHUMB)"
	cd $< && sed -i.orig "s/shrext_cmds/shrext/g" libtool
	cd $</libfaad && $(MAKE) install
	touch $@
