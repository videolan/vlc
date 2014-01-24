# libvorbis

VORBIS_VERSION := 1.3.4
VORBIS_URL := http://downloads.xiph.org/releases/vorbis/libvorbis-$(VORBIS_VERSION).tar.xz
#VORBIS_URL := $(CONTRIB_VIDEOLAN)/libvorbis-$(VORBIS_VERSION).tar.gz

ifdef HAVE_FPU
PKGS += vorbis
endif
ifeq ($(call need_pkg,"vorbis >= 1.1"),)
PKGS_FOUND += vorbis
endif
PKGS_ALL += vorbisenc
ifdef BUILD_ENCODERS
PKGS += vorbisenc
endif
ifeq ($(call need_pkg,"vorbisenc >= 1.1"),)
PKGS_FOUND += vorbisenc
endif

$(TARBALLS)/libvorbis-$(VORBIS_VERSION).tar.xz:
	$(call download,$(VORBIS_URL))

.sum-vorbis: libvorbis-$(VORBIS_VERSION).tar.xz

libvorbis: libvorbis-$(VORBIS_VERSION).tar.xz .sum-vorbis
	$(UNPACK)
	$(APPLY) $(SRC)/vorbis/osx.patch
	$(UPDATE_AUTOCONFIG)
	$(MOVE)

DEPS_vorbis = ogg $(DEPS_ogg)

.vorbis: libvorbis
	$(RECONF) -Im4
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-docs --disable-examples --disable-oggtest
	cd $< && $(MAKE) install
	touch $@

.sum-vorbisenc: .sum-vorbis
	touch $@

DEPS_vorbisenc = vorbis $(DEPS_vorbis)

.vorbisenc:
	touch $@
