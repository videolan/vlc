# AMF

AMF_VERSION := 1.4.36
AMF_URL := $(GITHUB)/GPUOpen-LibrariesAndSDKs/AMF/releases/download/v$(AMF_VERSION)/AMF-headers-v$(AMF_VERSION).tar.gz
AMF_GITURL := $(GITHUB)/GPUOpen-LibrariesAndSDKs/AMF.git
AMF_BRANCH := v$(AMF_VERSION)
AMF_GITVERSION := 8f5a645e89380549368eec68935b151b238aa17b

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
	$(call download,$(AMF_URL))

.sum-amf: AMF-$(AMF_VERSION).tar.gz

$(TARBALLS)/AMF-$(AMF_GITVERSION).tar.xz:
	$(RM) -Rf "$@" "$(@:.tar.xz=.githash)"
	$(RM) -Rf "$(@:.tar.xz=)"
	mkdir "$(@:.tar.xz=)"
	# clone the top of the branch and only checkout amf/public/include
	cd "$(@:.tar.xz=)" && git clone -n --depth=1 --filter=blob:none --no-checkout --branch $(AMF_BRANCH) $(AMF_GITURL) "$(notdir $(@:.tar.xz=))"
	cd "$(@:.tar.xz=)/$(notdir $(@:.tar.xz=))" && git config core.sparseCheckout true && echo "amf/public/include" >> .git/info/sparse-checkout && git checkout
	cd "$(@:.tar.xz=)" && tar cJf "$(notdir $(@))" --exclude=$(notdir $(@:.tar.xz=))/.git $(notdir $(@:.tar.xz=))
	cd "$(@:.tar.xz=)/$(notdir $(@:.tar.xz=))" && echo "`git rev-parse HEAD` $(@)" > "../tmp.githash"
	mv -f -- "$(@:.tar.xz=)/tmp.githash" "$(@:.tar.xz=.githash)"
	mv -f -- "$(@:.tar.xz=)/$(notdir $(@))" "$@"
	$(RM) -Rf "$(@:.tar.xz=)"

# .sum-amf: AMF-$(AMF_GITVERSION).tar.xz
# 	$(call check_githash,$(AMF_GITVERSION))
# 	touch "$@"

amf: UNPACK_DIR=amf-headers-v$(AMF_VERSION)
amf: AMF-$(AMF_VERSION).tar.gz .sum-amf
# amf: AMF-$(AMF_GITVERSION).tar.xz .sum-amf
	$(RM) -Rf AMF
	$(UNPACK)
	$(MOVE)

.amf: amf
	$(RM) -Rf $(PREFIX)/include/AMF
	mkdir -p $(PREFIX)/include/AMF
	cp -R $</* $(PREFIX)/include/AMF
	touch $@
