# jpeg

OPENJPEG_VERSION := 2.3.0
OPENJPEG_URL := https://github.com/uclouvain/openjpeg/archive/v$(OPENJPEG_VERSION).tar.gz

ifdef HAVE_WIN32
DEPS_openjpeg += pthreads $(DEPS_pthreads)
endif

$(TARBALLS)/openjpeg-v$(OPENJPEG_VERSION).tar.gz:
	$(call download_pkg,$(OPENJPEG_URL),openjpeg)

.sum-openjpeg: openjpeg-v$(OPENJPEG_VERSION).tar.gz

openjpeg: openjpeg-v$(OPENJPEG_VERSION).tar.gz .sum-openjpeg
	$(UNPACK)
	mv openjpeg-$(OPENJPEG_VERSION) openjpeg-v$(OPENJPEG_VERSION)
ifdef HAVE_VISUALSTUDIO
#	$(APPLY) $(SRC)/openjpeg/msvc.patch
endif
#	$(APPLY) $(SRC)/openjpeg/restrict.patch
	$(APPLY) $(SRC)/openjpeg/install.patch
	$(APPLY) $(SRC)/openjpeg/pic.patch
	$(APPLY) $(SRC)/openjpeg/openjp2_pthread.patch
	$(call pkg_static,"./src/lib/openjp2/libopenjp2.pc.cmake.in")
	$(MOVE)

.openjpeg: openjpeg toolchain.cmake
	cd $< && $(HOSTVARS) $(CMAKE) \
		-DBUILD_PKGCONFIG_FILES=ON \
			-DBUILD_CODEC:bool=OFF \
		.
	cd $< && $(MAKE) install
	touch $@
