# tremor (fixed-point Vorbis)

ifndef HAVE_FPU
PKGS += tremor
endif

$(TARBALLS)/tremor-svn.tar.xz:
	rm -Rf tremor-svn
	$(SVN) export http://svn.xiph.org/trunk/Tremor tremor-svn
	tar cvJ tremor-svn > $@

.sum-tremor: tremor-svn.tar.xz
	$(warning Integrity check skipped.)
	touch $@

tremor: tremor-svn.tar.xz .sum-tremor
	# Stuff that does not depend on libogg
	$(UNPACK)
	$(APPLY) $(SRC)/tremor/tremor.patch
	rm -f tremor-svn/ogg.h tremor-svn/os_types.h
	echo '#include <ogg/ogg.h>' > tremor-svn/ogg.h
	echo '#include <ogg/os_types.h>' > tremor-svn/os_types.h
	$(MOVE)

DEPS_tremor = ogg $(DEPS_ogg)

.tremor: tremor
	# Stuff that depends on libogg
	$(RECONF)
	cd $< && \
	$(HOSTVARS) CFLAGS="$(CFLAGS) $(NOTHUMB)" ./configure $(HOSTCONF)
	cd $< && $(MAKE) install
	touch $@
