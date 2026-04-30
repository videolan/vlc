# Theora

THEORA_VERSION := 1.2.0
THEORA_URL := $(XIPH)/theora/libtheora-$(THEORA_VERSION).tar.xz
THEORA_GITURL := https://gitlab.xiph.org/xiph/theora.git
THEORA_GITBRANCH := main
THEORA_GITVERSION := fb92ede9ba5162d0b8134cd1ff57751df6f3dbe6

PKGS += theora
ifeq ($(call need_pkg,"theora >= 1.0"),)
PKGS_FOUND += theora
endif

$(TARBALLS)/libtheora-$(THEORA_VERSION).tar.xz:
	$(call download_pkg,$(THEORA_URL),theora)

.sum-theora: libtheora-$(THEORA_VERSION).tar.xz

$(TARBALLS)/libtheora-$(THEORA_GITVERSION).tar.xz:
	$(call download_git,$(THEORA_GITURL),$(THEORA_GITBRANCH),$(THEORA_GITVERSION))

.sum-theora: libtheora-$(THEORA_GITVERSION).tar.xz
	$(call check_githash,$(THEORA_GITVERSION))
	touch $@

# libtheora: libtheora-$(THEORA_VERSION).tar.xz .sum-theora
libtheora: libtheora-$(THEORA_GITVERSION).tar.xz
	$(UNPACK)
	# $(call update_autoconfig,.)
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
	$(RECONF)
	$(MAKEBUILDDIR)
	$(MAKECONFIGURE) $(THEORACONF)
	+$(MAKEBUILD)
	+$(MAKEBUILD) install
	touch $@
