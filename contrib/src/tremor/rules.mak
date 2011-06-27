# tremor (fixed-point Vorbis)

$(TARBALLS)/tremor-svn.tar.xz:
	rm -Rf tremor-svn
	$(SVN) export http://svn.xiph.org/trunk/Tremor tremor-svn
	tar cv tremor-svn | xz > $@

.sum-tremor: $(TARBALLS)/tremor-svn.tar.xz
	$(warning Integrity check skipped.)
	touch $@

tremor: $(TARBALLS)/tremor-svn.tar.xz .sum-tremor
	# Stuff that does not depend on libogg
	$(UNPACK_XZ)
	(cd tremor-svn && patch -p0) < $(SRC)/tremor/tremor.patch
	rm -f tremor-svn/ogg.h tremor-svn/os_types.h
	echo '#include <ogg/ogg.h>' > tremor-svn/ogg.h
	echo '#include <ogg/os_types.h>' > tremor-svn/os_types.h
	mv tremor-svn tremor

.tremor: tremor .ogg
	# Stuff that depends on libogg
	cd $< && \
	$(HOSTVARS) ./autogen.sh $(HOSTCONF) \
		--prefix="$(PREFIX)" --disable-shared CFLAGS="$(NOTHUMB)"
	cd $< && $(MAKE) install
	touch $@
