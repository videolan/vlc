# Assimp

ASSIMP_VERSION := 4.1.0
ASSIMP_URL = https://github.com/assimp/assimp/archive/v$(ASSIMP_VERSION).tar.gz

DEPS_assimp = zlib

PKGS += assimp

ASSIMP_OPTIONS = \
	-DASSIMP_BUILD_ASSIMP_TOOLS=OFF \
	-DBUILD_SHARED_LIBS=OFF \
	-DASSIMP_BUILD_TESTS=OFF \
	-DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF \
	-DASSIMP_BUILD_FBX_IMPORTER=ON

ifeq ($(call need_pkg,"assimp"),)
PKGS_FOUND += assimp
endif

$(TARBALLS)/assimp-$(ASSIMP_VERSION).tar.gz:
	$(call download_pkg,$(ASSIMP_URL),assimp)

assimp: assimp-$(ASSIMP_VERSION).tar.gz
	$(UNPACK)
	$(APPLY) $(SRC)/assimp/0001-FBXConverter-fix-light-and-camera-position-convertio.patch
	$(MOVE)

.assimp: assimp toolchain.cmake
	-cd $< && rm CMakeCache.txt
	cd $< && $(HOSTVARS) $(CMAKE) $(ASSIMP_OPTIONS) -DCMAKE_INSTALL_PREFIX:PATH=$(PREFIX)
	cd $< && $(MAKE) install
	touch $@

