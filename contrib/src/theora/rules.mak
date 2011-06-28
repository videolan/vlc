# Theora

THEORA_VERSION := 1.1.1
THEORA_URL := http://downloads.xiph.org/releases/theora/libtheora-$(THEORA_VERSION).tar.xz
#THEORA_URL := $(CONTRIB_VIDEOLAN)/libtheora-$(THEORA_VERSION).tar.xz

PKGS += theora

$(TARBALLS)/libtheora-$(THEORA_VERSION).tar.xz:
	$(DOWNLOAD) $(THEORA_URL)

.sum-theora: libtheora-$(THEORA_VERSION).tar.xz

libtheora: libtheora-$(THEORA_VERSION).tar.xz .sum-theora
	$(UNPACK_XZ)
	(cd $@-$(THEORA_VERSION) && patch -p1) < $(SRC)/theora/libtheora-includes.patch
ifdef HAVE_WIN64
	cd $@ && autoreconf -fi -I m4
endif
	mv $@-$(THEORA_VERSION) $@
	touch $@

THEORACONF := $(HOSTCONF) \
	--disable-spec \
	--disable-sdltest \
	--disable-oggtest \
	--disable-vorbistest \
	--disable-examples

ifndef BUILD_ENCODERS
THEORACONF += --disable-encode
endif
ifndef HAVE_FPU
THEORACONF += --disable-float
endif
ifdef HAVE_MACOSX64
THEORACONF += --disable-asm
endif
ifdef HAVE_WIN64
THEORACONF += --disable-asm
endif

.theora: libtheora .ogg
ifdef HAVE_WIN32
	cd $<; $(HOSTVARS) ./autogen.sh $(THEORACONF)
endif
	test -f $</config.status || \
	(cd $<  && $(HOSTVARGS) ./configure $(THEORACONF))
	cd $< && $(MAKE) install
	touch $@
