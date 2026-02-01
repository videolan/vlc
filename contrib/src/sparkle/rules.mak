# Sparkle

SPARKLE_VERSION := 2.8.1
SPARKLE_URL := $(GITHUB)/sparkle-project/Sparkle/archive/$(SPARKLE_VERSION).zip

ifdef HAVE_MACOSX
PKGS += sparkle
endif

$(TARBALLS)/Sparkle-$(SPARKLE_VERSION).zip:
	$(call download_pkg,$(SPARKLE_URL),sparkle)

.sum-sparkle: Sparkle-$(SPARKLE_VERSION).zip

sparkle: Sparkle-$(SPARKLE_VERSION).zip .sum-sparkle
	$(UNPACK)
	$(APPLY) $(SRC)/sparkle/macos12-compilation.patch
	$(MOVE)

.sparkle: sparkle
	# Build Sparkle and change the @rpath
	cd $< && xcodebuild $(XCODE_FLAGS)
	# Remove XPC service which are only needed on sandboxed apps
	cd $< && rm -Rf build/Release/Sparkle.framework/Versions/B/XPCServices
	cd $< && rm -Rf build/Release/Sparkle.framework/XPCServices
	# Install
	cd $< && mkdir -p "$(PREFIX)/Frameworks" && \
		rm -Rf "$(PREFIX)/Frameworks/Sparkle.framework" && \
		cp -R build/Release/Sparkle.framework "$(PREFIX)/Frameworks"
	touch $@
