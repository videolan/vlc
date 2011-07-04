# Theora

THEORA_VERSION := 1.1.1
THEORA_URL := http://downloads.xiph.org/releases/theora/libtheora-$(THEORA_VERSION).tar.xz
#THEORA_URL := $(CONTRIB_VIDEOLAN)/libtheora-$(THEORA_VERSION).tar.xz

PKGS += theora

$(TARBALLS)/libtheora-$(THEORA_VERSION).tar.xz:
	$(call download,$(THEORA_URL))

.sum-theora: libtheora-$(THEORA_VERSION).tar.xz

libtheora: libtheora-$(THEORA_VERSION).tar.xz .sum-theora
	$(UNPACK)
	$(APPLY) $(SRC)/theora/libtheora-includes.patch
	echo 'ACLOCAL_AMFLAGS = -I m4' >> $(UNPACK_DIR)/Makefile.am
	$(MOVE)

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

DEPS_theora = ogg $(DEPS_ogg)

.theora: libtheora
ifdef HAVE_WIN32
	$(RECONF)
endif
	cd $< && $(HOSTVARGS) ./configure $(THEORACONF)
	cd $< && $(MAKE) install
	touch $@
