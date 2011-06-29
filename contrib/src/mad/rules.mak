# mad

MAD_VERSION := 0.15.1b
MAD_URL := $(CONTRIB_VIDEOLAN)/libmad-$(MAD_VERSION).tar.gz

PKGS += mad

$(TARBALLS)/libmad-$(MAD_VERSION).tar.gz:
	$(call download,$(MAD_URL))

.sum-mad: libmad-$(MAD_VERSION).tar.gz

libmad: libmad-$(MAD_VERSION).tar.gz .sum-mad
	$(UNPACK)
ifdef HAVE_MACOSX
	cd $@-$(MAD_VERSION) && sed \
		-e 's%-march=i486%$(EXTRA_CFLAGS) $(EXTRA_LDFLAGS)%' \
		-e 's%-dynamiclib%-dynamiclib -arch $(ARCH)%' \
		-i.orig configure
endif
	mv $@-$(MAD_VERSION) $@
	touch $@

.mad: libmad
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -O3 $(NOTHUMB)" ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
