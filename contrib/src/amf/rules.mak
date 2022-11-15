# AMF

AMF_VERSION := 1.4.26
AMF_URL := $(GITHUB)/GPUOpen-LibrariesAndSDKs/AMF/archive/refs/tags/v$(AMF_VERSION).tar.gz

ifeq ($(ARCH),x86_64)
ifdef HAVE_WIN32
PKGS += amf
endif
ifdef HAVE_LINUX
ifndef HAVE_ANDROID
PKGS += amf
endif
endif
endif

$(TARBALLS)/AMF-$(AMF_VERSION).tar.gz:
	$(call download_pkg,$(AMF_URL),AMF)

.sum-amf: AMF-$(AMF_VERSION).tar.gz

amf: AMF-$(AMF_VERSION).tar.gz .sum-amf
	$(UNPACK)
	$(MOVE)

.amf: amf
	mkdir -p $(PREFIX)/include/AMF
	cp -R $(UNPACK_DIR)/amf/public/include/* $(PREFIX)/include/AMF
	touch $@
