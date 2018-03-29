# Assimp

ASSIMP_VERSION := 4.1.0
ASSIMP_URL = https://github.com/assimp/assimp/archive/v$(ASSIMP_VERSION).tar.gz 

DEPS_assimp = zlib

PKGS += assimp

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
	cd $< && $(HOSTVARS) $(CMAKE) -DASSIMP_BUILD_ASSIMP_TOOLS=OFF -DBUILD_SHARED_LIBS=OFF
	cd $< && $(MAKE) install
	touch $@

