# sparkle

#SPARKLE_VERSION := 1.5b6
#SPARKLE_URL := http://sparkle.andymatuschak.org/files/Sparkle%20$(SPARKLE_VERSION).zip
SPARKLE_GITURL := git://github.com/andymatuschak/Sparkle.git

ifdef HAVE_MACOSX
PKGS += sparkle
endif

$(TARBALLS)/sparkle-git.tar.xz:
	$(call download_git,$(SPARKLE_GITURL),,HEAD)

.sum-sparkle: sparkle-git.tar.xz
	$(warning $@ not implemented)
	touch $@

sparkle: sparkle-git.tar.xz .sum-sparkle
	$(UNPACK)
	$(APPLY) $(SRC)/sparkle/sparkle-fix-formatstring.patch
	$(MOVE)

.sparkle: sparkle
	cd $< && xcodebuild $(XCODE_FLAGS)
	cd $< && install_name_tool -id @executable_path/../Frameworks/Sparkle.framework/Versions/A/Sparkle build/Release/Sparkle.framework/Sparkle
	cd $< && cp -R build/Release/Sparkle.framework "$(PREFIX)"
	touch $@
