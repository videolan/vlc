# AMF

AMF_VERSION := 1.4.30
AMF_URL := $(GITHUB)/GPUOpen-LibrariesAndSDKs/AMF/archive/refs/tags/v$(AMF_VERSION).tar.gz
AMF_GITURL := $(GITHUB)/GPUOpen-LibrariesAndSDKs/AMF.git
AMF_BRANCH := v$(AMF_VERSION)
AMF_GITVERSION := a118570647cfa579af8875c3955a314c3ddd7058

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

$(TARBALLS)/AMF-$(AMF_GITVERSION).tar.xz:
	rm -rf "$@" "$(@:.tar.xz=.githash)"
	rm -rf "$(@:.tar.xz=)"
	mkdir "$(@:.tar.xz=)"
	# clone the top of the branch and only checkout amf/public/include
	cd "$(@:.tar.xz=)" && git clone -n --depth=1 --filter=tree:0 --branch $(AMF_BRANCH) $(AMF_GITURL) "$(notdir $(@:.tar.xz=))"
	cd "$(@:.tar.xz=)/$(notdir $(@:.tar.xz=))" && git sparse-checkout set --no-cone amf/public/include && git checkout
	cd "$(@:.tar.xz=)" && tar cJf "$(notdir $(@))" --exclude=$(notdir $(@:.tar.xz=))/.git $(notdir $(@:.tar.xz=))
	cd "$(@:.tar.xz=)/$(notdir $(@:.tar.xz=))" && echo "`git rev-parse HEAD` $(@)" > "../tmp.githash"
	mv -f -- "$(@:.tar.xz=)/tmp.githash" "$(@:.tar.xz=.githash)"
	mv -f -- "$(@:.tar.xz=)/$(notdir $(@))" "$@"
	rm -rf "$(@:.tar.xz=)"

.sum-amf: AMF-$(AMF_GITVERSION).tar.xz
	$(call check_githash,$(AMF_GITVERSION))
	touch "$@"

# amf: AMF-$(AMF_VERSION).tar.gz .sum-amf
amf: AMF-$(AMF_GITVERSION).tar.xz .sum-amf
	$(UNPACK)
	$(MOVE)

.amf: amf
	mkdir -p $(PREFIX)/include/AMF
	cp -R $(UNPACK_DIR)/amf/public/include/* $(PREFIX)/include/AMF
	touch $@
