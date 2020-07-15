# libvorbis

VORBIS_VERSION := 1.3.6
VORBIS_URL := http://downloads.xiph.org/releases/vorbis/libvorbis-$(VORBIS_VERSION).tar.xz

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
ifdef HAVE_CLANG
	$(APPLY) $(SRC)/vorbis/clang.patch
endif
	$(UPDATE_AUTOCONFIG)
	$(APPLY) $(SRC)/vorbis/vorbis-bitcode.patch
	$(call pkg_static,"vorbis.pc.in")
	$(MOVE)

DEPS_vorbis = ogg $(DEPS_ogg)

.vorbis: libvorbis
	$(RECONF) -Im4
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-docs --disable-examples --disable-oggtest
	cd $< && $(MAKE) install
	touch $@
