# mad

MAD_VERSION := 0.15.1b
MAD_URL := $(CONTRIB_VIDEOLAN)/libmad-$(MAD_VERSION).tar.gz

ifdef GPL
PKGS += mad
endif
ifeq ($(call need_pkg,"mad"),)
PKGS_FOUND += mad
endif

$(TARBALLS)/libmad-$(MAD_VERSION).tar.gz:
	$(call download,$(MAD_URL))

.sum-mad: libmad-$(MAD_VERSION).tar.gz

libmad: libmad-$(MAD_VERSION).tar.gz .sum-mad
	$(UNPACK)
ifdef HAVE_DARWIN_OS
	cd $@-$(MAD_VERSION) && sed \
		-e 's%-march=i486%$(EXTRA_CFLAGS) $(EXTRA_LDFLAGS)%' \
		-e 's%-dynamiclib%-dynamiclib -arch $(ARCH)%' \
		-i.orig configure
endif
ifdef HAVE_IOS
	$(APPLY) $(SRC)/mad/mad-ios-asm.patch
endif
	$(APPLY) $(SRC)/mad/mad-noopt.patch
	$(APPLY) $(SRC)/mad/Provide-Thumb-2-alternative-code-for-MAD_F_MLN.diff
	$(APPLY) $(SRC)/mad/mad-mips-h-constraint-removal.patch
	$(MOVE)

.mad: libmad
	touch libmad/NEWS libmad/AUTHORS libmad/ChangeLog
	$(RECONF)
ifdef HAVE_IOS
	cd $< && $(HOSTVARS) CCAS="$(AS)" CFLAGS="$(CFLAGS) -O3" ./configure $(HOSTCONF) $(MAD_CONF)
else
	cd $< && $(HOSTVARS) CFLAGS="$(CFLAGS) -O3" ./configure $(HOSTCONF) $(MAD_CONF)
endif
	cd $< && $(MAKE) install
	touch $@
