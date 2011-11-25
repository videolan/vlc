# sparkle

SPARKLE_VERSION := 1.5b6
SPARKLE_URL := http://sparkle.andymatuschak.org/files/Sparkle%20$(SPARKLE_VERSION).zip

ifdef HAVE_MACOSX
PKGS += sparkle
endif

$(TARBALLS)/sparkle-$(SPARKLE_VERSION).zip:
	$(call download,$(SPARKLE_URL))

.sum-sparkle: sparkle-$(SPARKLE_VERSION).zip

sparkle: sparkle-$(SPARKLE_VERSION).zip .sum-sparkle
	$(RM) -R $@ && mkdir -p $@ && cd $@ && unzip ../$<
	cd $@/Extras/Source\ Code/Configurations && \
		sed -i.orig -e s/"GCC_TREAT_WARNINGS_AS_ERRORS = YES"/"GCC_TREAT_WARNINGS_AS_ERRORS = NO"/g \
			ConfigCommonRelease.xcconfig && \
		sed -i.orig -e s/10\.4/$(OSX_VERSION)/g -e s/10\.5/$(OSX_VERSION)/g ConfigCommon.xcconfig
	touch $@

.sparkle: sparkle
	cd $</Extras/Source\ Code && $(MAKE) && xcodebuild
	cd $< && cp -R -L Extras/Source\ Code/build/release/Sparkle.framework "$(PREFIX)"
	touch $@
