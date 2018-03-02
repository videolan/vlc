# Sparkle

SPARKLE_VERSION := 1.16.0
SPARKLE_URL := https://github.com/sparkle-project/Sparkle/archive/$(SPARKLE_VERSION).zip

ifdef HAVE_MACOSX
PKGS += sparkle
endif

$(TARBALLS)/Sparkle-$(SPARKLE_VERSION).zip:
	$(call download_pkg,$(SPARKLE_URL),sparkle)

.sum-sparkle: Sparkle-$(SPARKLE_VERSION).zip

sparkle: Sparkle-$(SPARKLE_VERSION).zip .sum-sparkle
	$(UNPACK)
	$(MOVE)

.sparkle: sparkle
	# Build Sparkle and change the @rpath
	cd $< && xcodebuild $(XCODE_FLAGS)
	cd $< && install_name_tool -id @executable_path/../Frameworks/Sparkle.framework/Versions/A/Sparkle build/Release/Sparkle.framework/Sparkle
	# Install
	cd $< && mkdir -p "$(PREFIX)/Frameworks" && \
		rm -Rf "$(PREFIX)/Frameworks/Sparkle.framework" && \
		cp -R build/Release/Sparkle.framework "$(PREFIX)/Frameworks"
	touch $@
