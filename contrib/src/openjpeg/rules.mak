# jpeg

OPENJPEG_VERSION := 2.5.4
OPENJPEG_URL := $(GITHUB)/uclouvain/openjpeg/archive/v$(OPENJPEG_VERSION).tar.gz

ifdef HAVE_WIN32
DEPS_openjpeg += winpthreads $(DEPS_winpthreads)
endif

$(TARBALLS)/openjpeg-$(OPENJPEG_VERSION).tar.gz:
	$(call download_pkg,$(OPENJPEG_URL),openjpeg)

.sum-openjpeg: openjpeg-$(OPENJPEG_VERSION).tar.gz

openjpeg: openjpeg-$(OPENJPEG_VERSION).tar.gz .sum-openjpeg
	$(UNPACK)
	$(APPLY) $(SRC)/openjpeg/5e258319332800f7a9937dc0b8b16b19a07dea8f.patch
	$(APPLY) $(SRC)/openjpeg/7b508bb00f7fc5e7b61a6035fc4e2622d4ddff0d.patch
	$(call pkg_static,"./src/lib/openjp2/libopenjp2.pc.cmake.in")
	$(MOVE)

OPENJPEG_CONF := -DBUILD_PKGCONFIG_FILES=ON -DBUILD_CODEC:bool=OFF

.openjpeg: openjpeg toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(OPENJPEG_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
