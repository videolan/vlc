# libvorbis

VORBIS_VERSION := 1.3.2
VORBIS_URL := http://downloads.xiph.org/releases/vorbis/libvorbis-$(VORBIS_VERSION).tar.xz
#VORBIS_URL := $(CONTRIB_VIDEOLAN)/libvorbis-$(VORBIS_VERSION).tar.gz

ifndef HAVE_FPU
PKGS += vorbis
endif
ifdef BUILD_ENCODERS
PKGS += vorbisenc
endif

$(TARBALLS)/libvorbis-$(VORBIS_VERSION).tar.xz:
	$(call download,$(VORBIS_URL))

.sum-vorbis: libvorbis-$(VORBIS_VERSION).tar.xz

libvorbis: libvorbis-$(VORBIS_VERSION).tar.xz .sum-vorbis
	$(UNPACK)
	mv $@-$(VORBIS_VERSION) $@
	touch $@

.vorbis: libvorbis .ogg
	cd $< && $(HOSTVARS) ./configure $(HOSTCONF) --disable-docs --disable-examples --disable-oggtest
	cd $< && $(MAKE) install
	touch $@

.sum-vorbisenc: .sum-vorbis
	touch $@

.vorbisenc: .vorbis
	touch $@
