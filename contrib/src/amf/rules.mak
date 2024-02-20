# AMF

AMF_VERSION := 1.4.33
AMF_URL := $(GITHUB)/GPUOpen-LibrariesAndSDKs/AMF/archive/refs/tags/v$(AMF_VERSION).tar.gz
AMF_GITURL := $(GITHUB)/GPUOpen-LibrariesAndSDKs/AMF.git
AMF_BRANCH := v$(AMF_VERSION)
AMF_GITVERSION := e8c7cd7c10d4e05c1913aa8dfd2be9f9dbdb03d6

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
	cd "$(@:.tar.xz=)" && git clone -n --depth=1 --no-checkout --branch $(AMF_BRANCH) $(AMF_GITURL) "$(notdir $(@:.tar.xz=))"
	cd "$(@:.tar.xz=)/$(notdir $(@:.tar.xz=))" && git config core.sparseCheckout true && echo "amf/public/include" >> .git/info/sparse-checkout && git checkout
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
	$(APPLY) $(SRC)/amf/0001-Move-AMF_UNICODE-into-Platform.h.patch
	$(APPLY) $(SRC)/amf/0002-Define-LPRI-d-ud-x-64-as-Unicode-wide-versions-of-AM.patch
	$(APPLY) $(SRC)/amf/0003-Define-AMFPRI-d-ud-x-64-using-the-standard-C-format-.patch
	$(APPLY) $(SRC)/amf/0001-Don-t-cast-amf_int64-when-using-a-format-string.patch
	$(APPLY) $(SRC)/amf/0001-Differentiate-the-AMF_NO_VTABLE-based-on-the-compile.patch
	$(APPLY) $(SRC)/amf/0001-Fix-const-on-return-by-value-AMF_DECLARE_IID.patch
	$(APPLY) $(SRC)/amf/0002-Fix-const-on-return-by-value-Variant-values.patch
	$(APPLY) $(SRC)/amf/0001-Fix-warning-when-_MSC_VER-is-not-defined.patch
	$(MOVE)

.amf: amf
	mkdir -p $(PREFIX)/include/AMF
	cp -R $(UNPACK_DIR)/amf/public/include/* $(PREFIX)/include/AMF
	touch $@
