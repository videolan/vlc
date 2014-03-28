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
	$(APPLY) $(SRC)/sparkle/sparkle-fix-xcode-project-for-current-releases.patch
	$(APPLY) $(SRC)/sparkle/sparkle-fix-compilation-on-snowleopard.patch
	$(MOVE)

.sparkle: sparkle
	cd $< && xcodebuild $(XCODE_FLAGS) WARNING_CFLAGS=-Wno-error
	cd $< && install_name_tool -id @executable_path/../Frameworks/Sparkle.framework/Versions/A/Sparkle build/Release/Sparkle.framework/Sparkle
	install -d $(PREFIX)
	cd $< && cp -R build/Release/Sparkle.framework "$(PREFIX)"
	touch $@
