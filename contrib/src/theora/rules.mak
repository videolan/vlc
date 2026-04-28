# Theora

THEORA_VERSION := 1.2.0
THEORA_URL := $(XIPH)/theora/libtheora-$(THEORA_VERSION).tar.xz

PKGS += theora
ifeq ($(call need_pkg,"theora >= 1.0"),)
PKGS_FOUND += theora
endif

$(TARBALLS)/libtheora-$(THEORA_VERSION).tar.xz:
	$(call download_pkg,$(THEORA_URL),theora)

.sum-theora: libtheora-$(THEORA_VERSION).tar.xz

libtheora: libtheora-$(THEORA_VERSION).tar.xz .sum-theora
	$(UNPACK)
	$(call update_autoconfig,.)
	$(MOVE)

THEORACONF := \
	--disable-spec \
	--disable-sdltest \
	--disable-oggtest \
	--disable-vorbistest \
	--disable-examples \
	--disable-doc

ifndef BUILD_ENCODERS
THEORACONF += --disable-encode
endif
ifndef HAVE_FPU
THEORACONF += --disable-float
endif
ifdef HAVE_IOS
THEORACONF += --disable-asm
endif

DEPS_theora = ogg $(DEPS_ogg)

.theora: libtheora
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(THEORACONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
