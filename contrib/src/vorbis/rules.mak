# libvorbis

VORBIS_VERSION := 1.3.7
VORBIS_URL := $(XIPH)/vorbis/libvorbis-$(VORBIS_VERSION).tar.xz

ifdef HAVE_FPU
PKGS += vorbis
endif
ifdef BUILD_ENCODERS
PKGS += vorbis
endif

ifeq ($(call need_pkg,"vorbis >= 1.1"),)
ifdef BUILD_ENCODERS
ifeq ($(call need_pkg,"vorbisenc >= 1.1"),)
PKGS_FOUND += vorbis
endif
else
PKGS_FOUND += vorbis
endif
endif

$(TARBALLS)/libvorbis-$(VORBIS_VERSION).tar.xz:
	$(call download_pkg,$(VORBIS_URL),vorbis)

.sum-vorbis: libvorbis-$(VORBIS_VERSION).tar.xz

libvorbis: libvorbis-$(VORBIS_VERSION).tar.xz .sum-vorbis
	$(UNPACK)
	$(APPLY) $(SRC)/vorbis/0001-CMake-add-missing-libm-in-.pc-file-when-it-s-used.patch
	$(call pkg_static,"vorbis.pc.in")
	$(call pkg_static,"vorbisenc.pc.in")
	$(call pkg_static,"vorbisfile.pc.in")
	$(MOVE)

DEPS_vorbis = ogg $(DEPS_ogg)

.vorbis: libvorbis toolchain.cmake
	$(CMAKECLEAN)
	$(HOSTVARS_CMAKE) $(CMAKE) -DCMAKE_POLICY_VERSION_MINIMUM=3.5
	+$(CMAKEBUILD)
	$(CMAKEINSTALL)
	touch $@
