# growl

GROWL_VERSION := 2.0.1
GROWL_URL := https://download.videolan.org/contrib/GrowlSDK-$(GROWL_VERSION)-src.tar.gz

ifdef HAVE_MACOSX
PKGS += growl
endif

$(TARBALLS)/GrowlSDK-$(GROWL_VERSION)-src.tar.gz:
	$(call download_pkg,$(GROWL_URL),growl)

.sum-growl: GrowlSDK-$(GROWL_VERSION)-src.tar.gz

growl: GrowlSDK-$(GROWL_VERSION)-src.tar.gz .sum-growl
	$(UNPACK)
	$(APPLY) $(SRC)/growl/fix-function-check.patch
	$(APPLY) $(SRC)/growl/security-nothanks.patch
	$(APPLY) $(SRC)/growl/growl-log-delegate.patch
	$(APPLY) $(SRC)/growl/growl-partial-availability.diff
	$(APPLY) $(SRC)/growl/growl-update-vcs-target.patch
	$(APPLY) $(SRC)/growl/growl-support-arm64.patch
	$(MOVE)

.growl: growl
	cd $< && xcodebuild $(XCODE_FLAGS) MACOSX_DEPLOYMENT_TARGET=10.7 CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY="" GCC_TREAT_WARNINGS_AS_ERRORS=NO -target Growl.framework -configuration Release

	# Support for macOS 10.7 on intel builds
ifeq ($(ARCH),x86_64)
	cd $< && install_name_tool -change /System/Library/Frameworks/CFNetwork.framework/Versions/A/CFNetwork /System/Library/Frameworks/CoreServices.framework/Versions/A/CoreServices build/Release/Growl.framework/Growl
endif

	install -d $(PREFIX)
	cd $< && mkdir -p "$(PREFIX)/Frameworks" && rm -Rf $(PREFIX)/Frameworks/Growl.framework && \
	         cp -Rf build/Release/Growl.framework "$(PREFIX)/Frameworks"
	touch $@
