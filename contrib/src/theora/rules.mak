# Theora

THEORA_VERSION := 1.1.1
THEORA_URL := http://downloads.xiph.org/releases/theora/libtheora-$(THEORA_VERSION).tar.xz

PKGS += theora
ifeq ($(call need_pkg,"theora >= 1.0"),)
PKGS_FOUND += theora
endif

$(TARBALLS)/libtheora-$(THEORA_VERSION).tar.xz:
	$(call download_pkg,$(THEORA_URL),theora)

.sum-theora: libtheora-$(THEORA_VERSION).tar.xz

libtheora: libtheora-$(THEORA_VERSION).tar.xz .sum-theora
	$(UNPACK)
	$(APPLY) $(SRC)/theora/libtheora-compiler-differentiation.patch
	$(APPLY) $(SRC)/theora/libtheora-no-forceaddr.patch
	$(UPDATE_AUTOCONFIG)
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
ifdef HAVE_IOS
THEORACONF += --disable-asm
endif
ifdef HAVE_WIN32
ifeq ($(ARCH),x86_64)
THEORACONF += --disable-asm
endif
endif

DEPS_theora = ogg $(DEPS_ogg)

.theora: libtheora
	cd $< && $(HOSTVARS) ./configure $(THEORACONF)
	cd $< && $(MAKE) install
	touch $@
