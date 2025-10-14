# Sparkle

SPARKLE_VERSION := 1.16.0
SPARKLE_URL := $(GITHUB)/sparkle-project/Sparkle/archive/$(SPARKLE_VERSION).zip

ifdef HAVE_MACOSX
# fails to build on newer SDK because of missing libarclite (found in 13.1, missing in 13.3 from XCode 14.3)
ifeq ($(call darwin_sdk_at_most, 13.1), true)
CAN_BUILD_SPARKLE:=1
endif
ifeq ($(call darwin_min_os_at_least, 10.11), true)
# builds when targeting macOS 10.11
CAN_BUILD_SPARKLE:=1
endif
endif

ifdef CAN_BUILD_SPARKLE
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
	# Install
	cd $< && mkdir -p "$(PREFIX)/Frameworks" && \
		rm -Rf "$(PREFIX)/Frameworks/Sparkle.framework" && \
		cp -R build/Release/Sparkle.framework "$(PREFIX)/Frameworks"
	touch $@
