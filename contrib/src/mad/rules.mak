# mad

MAD_VERSION := 0.15.1b
MAD_URL := $(CONTRIB_VIDEOLAN)/mad/libmad-$(MAD_VERSION).tar.gz

ifdef GPL
PKGS += mad
endif
ifeq ($(call need_pkg,"mad"),)
PKGS_FOUND += mad
endif

ifdef HAVE_WIN32
ifeq ($(ARCH),arm)
MAD_CONF += --disable-aso
endif
endif

$(TARBALLS)/libmad-$(MAD_VERSION).tar.gz:
	$(call download,$(MAD_URL))

LIBMAD_VARS := CFLAGS="$(CFLAGS) -O3"
ifdef HAVE_IOS
LIBMAD_VARS += CCAS="$(AS)"
endif

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
	$(APPLY) $(SRC)/mad/mad-foreign.patch
	$(APPLY) $(SRC)/mad/check-bitstream-length.patch
	cd $(UNPACK_DIR) && rm -rf aclocal.m4 Makefile.in
	$(MOVE)

.mad: libmad
	$(REQUIRE_GPL)
	$(RECONF)
	cd $< && $(HOSTVARS) $(LIBMAD_VARS) ./configure $(HOSTCONF) $(MAD_CONF)
	cd $< && $(MAKE) install
	touch $@
