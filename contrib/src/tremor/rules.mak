# tremor (fixed-point Vorbis)

TREMOR_URL := https://gitlab.xiph.org/xiph/tremor.git
TREMOR_HASH := b56ffce0

ifndef HAVE_FPU
PKGS += tremor
endif

$(TARBALLS)/tremor-git.tar.xz:
	$(call download_git,$(TREMOR_URL),master,$(TREMOR_HASH))

.sum-tremor: tremor-git.tar.xz
	$(warning Integrity check skipped.)
	touch $@

tremor: tremor-git.tar.xz .sum-tremor
	# Stuff that does not depend on libogg
	$(UNPACK)
	$(APPLY) $(SRC)/tremor/tremor.patch
	$(MOVE)

DEPS_tremor = ogg $(DEPS_ogg)

.tremor: tremor
	# Stuff that depends on libogg
	$(RECONF)
	cd $< && \
	$(HOSTVARS) CFLAGS="$(CFLAGS) $(NOTHUMB)" ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
