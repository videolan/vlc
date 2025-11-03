# openapv

OPENAPV_VERSION := 0.2.0.4
OPENAPV_URL := $(GITHUB)/AcademySoftwareFoundation/openapv/archive/refs/tags/v$(OPENAPV_VERSION).tar.gz

PKGS += openapv
ifeq ($(call need_pkg,"oapv >= 0.2"),)
PKGS_FOUND += openapv
endif

$(TARBALLS)/openapv-$(OPENAPV_VERSION).tar.gz:
	$(call download_pkg,$(OPENAPV_URL),openapv)

.sum-openapv: openapv-$(OPENAPV_VERSION).tar.gz

OPENAPV_CONF := -DOAPV_BUILD_SHARED_LIB=OFF -DOAPV_BUILD_APPS=OFF

openapv: openapv-$(OPENAPV_VERSION).tar.gz .sum-openapv
	$(UNPACK)
	$(call pkg_static,"pkgconfig/oapv.pc.in")
	# install the library in the usual <prefix>/lib place to match the .pc file
	sed -i.orig 's,$${CMAKE_INSTALL_LIBDIR}/$${LIB_NAME_BASE},$${CMAKE_INSTALL_LIBDIR},g' $(UNPACK_DIR)/src/CMakeLists.txt
	$(MOVE)

.openapv: openapv toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) $(OPENAPV_CONF)
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
