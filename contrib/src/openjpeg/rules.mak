# jpeg

OPENJPEG_VERSION := 2.5.0
OPENJPEG_URL := $(GITHUB)/uclouvain/openjpeg/archive/v$(OPENJPEG_VERSION).tar.gz

ifdef HAVE_WIN32
DEPS_openjpeg += pthreads $(DEPS_pthreads)
endif

$(TARBALLS)/openjpeg-v$(OPENJPEG_VERSION).tar.gz:
	$(call download_pkg,$(OPENJPEG_URL),openjpeg)

.sum-openjpeg: openjpeg-v$(OPENJPEG_VERSION).tar.gz

openjpeg: openjpeg-v$(OPENJPEG_VERSION).tar.gz .sum-openjpeg
	$(UNPACK)
	mv openjpeg-$(OPENJPEG_VERSION) openjpeg-v$(OPENJPEG_VERSION)
	$(APPLY) $(SRC)/openjpeg/pic.patch
	$(APPLY) $(SRC)/openjpeg/openjp2_pthread.patch
	$(call pkg_static,"./src/lib/openjp2/libopenjp2.pc.cmake.in")
	$(MOVE)

OPENJPEG_CONF := -DBUILD_PKGCONFIG_FILES=ON -DBUILD_CODEC:bool=OFF

.openjpeg: openjpeg toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS) $(CMAKE) $(OPENJPEG_CONF)
	+$(CMAKEBUILD) --target install
	touch $@
