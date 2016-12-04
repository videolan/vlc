if HAVE_DARWIN
if BUILD_MACOSX_VLC_APP
# Create the MacOS X app
noinst_DATA = VLC.app
endif
endif

# This is just for development purposes.
# The resulting VLC-dev.app will only run in this tree.
VLC-dev.app: VLC-tmp
	rm -Rf $@
	cp -R VLC-tmp $@
	$(INSTALL) -m 0755 $(top_builddir)/bin/.libs/vlc-osx $@/Contents/MacOS/VLC
	$(LN_S) -f ../../../modules $@/Contents/MacOS/plugins

# VLC.app for packaging and giving it to your friends
# use package-macosx to get a nice dmg
VLC.app: VLC-tmp
	rm -Rf $@
	cp -R VLC-tmp $@
	rm -Rf $@/Contents/Frameworks/BGHUDAppKit.framework/Versions/A/Resources/BGHUDAppKitPlugin.ibplugin
	rm -Rf $@/Contents/Frameworks/BGHUDAppKit.framework/Versions/A/Resources/README.textile
	PRODUCT="$@" ACTION="release-makefile" src_dir=$(srcdir) build_dir=$(top_builddir) sh $(srcdir)/extras/package/macosx/build-package.sh
	bin/vlc-cache-gen $@/Contents/MacOS/plugins
	find $@ -type d -exec chmod ugo+rx '{}' \;
	find $@ -type f -exec chmod ugo+r '{}' \;

VLC-tmp:
	$(AM_V_GEN)for i in src lib share; do \
		(cd $$i && $(MAKE) $(AM_MAKEFLAGS) install $(silentstd)); \
	done
	rm -Rf "$(top_builddir)/tmp" "$@"
	mkdir -p "$(top_builddir)/tmp/extras/package/macosx"
	cd $(srcdir)/extras/package/macosx; cp -R Resources $(abs_top_builddir)/tmp/extras/package/macosx/
	mkdir -p $(abs_top_builddir)/tmp/extras/package/macosx/vlc.xcodeproj/
	sed -e s,../../../contrib,$(CONTRIB_DIR),g $(srcdir)/extras/package/macosx/vlc.xcodeproj/project.pbxproj \
        > $(abs_top_builddir)/tmp/extras/package/macosx/vlc.xcodeproj/project.pbxproj
	REVISION=`(git --git-dir=$(srcdir)/.git describe --always || echo exported)` && \
	    sed "s/#REVISION#/$$REVISION/g" $(top_builddir)/extras/package/macosx/Info.plist \
        > $(top_builddir)/tmp/extras/package/macosx/Info.plist
	xcrun plutil -convert binary1 $(top_builddir)/tmp/extras/package/macosx/Info.plist
	cp -R $(top_builddir)/extras/package/macosx/Resources $(top_builddir)/tmp/extras/package/macosx/
	cd "$(srcdir)"; cp AUTHORS COPYING THANKS $(abs_top_builddir)/tmp/
	mkdir -p $(top_builddir)/tmp/modules/audio_output
	mkdir -p $(top_builddir)/tmp/modules/gui/macosx
	cd "$(srcdir)/modules/gui/macosx/" && cp *.h *.m $(abs_top_builddir)/tmp/modules/gui/macosx/
	cd $(top_builddir)/tmp/extras/package/macosx && \
		xcodebuild -target vlc-bundle-helper SYMROOT=../../../build DSTROOT=../../../build $(silentstd)
	cp -R $(top_builddir)/tmp/build/Default/VLC.bundle $@
	mkdir -p $@/Contents/Frameworks && cp -R $(CONTRIB_DIR)/Growl.framework $@/Contents/Frameworks/
if HAVE_SPARKLE
	cp -R $(CONTRIB_DIR)/Sparkle.framework $@/Contents/Frameworks/
endif
	mkdir -p $@/Contents/MacOS/share/locale/
if BUILD_LUA
	cp -r "$(prefix)/lib/vlc/lua" "$(prefix)/share/vlc/lua" $@/Contents/MacOS/share/
endif
	mkdir -p $@/Contents/MacOS/include/
	(cd "$(prefix)/include" && $(AMTAR) -c --exclude "plugins" vlc) | $(AMTAR) -x -C $@/Contents/MacOS/include/
	$(INSTALL) -m 644 $(srcdir)/share/vlc512x512.png $@/Contents/MacOS/share/vlc512x512.png
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  $(INSTALL) -d $@/Contents/MacOS/share/locale/$${i}/LC_MESSAGES ; \
	  $(INSTALL) $(srcdir)/po/$${i}.gmo $@/Contents/MacOS/share/locale/$${i}/LC_MESSAGES/vlc.mo; \
	  mkdir -p $@/Contents/Resources/$${i}.lproj/ ; \
	  $(LN_S) -f ../English.lproj/InfoPlist.strings ../English.lproj/MainMenu.nib \
		$@/Contents/Resources/$${i}.lproj/ ; \
	done
	printf "APPLVLC#" >| $@/Contents/PkgInfo

package-macosx: VLC.app
	mkdir -p "$(top_builddir)/vlc-$(VERSION)/Goodies/"
	cp -R "$(top_builddir)/VLC.app" "$(top_builddir)/vlc-$(VERSION)/VLC.app"
	cd $(srcdir); cp AUTHORS COPYING README THANKS NEWS $(abs_top_builddir)/vlc-$(VERSION)/Goodies/
	$(LN_S) -f /Applications $(top_builddir)/vlc-$(VERSION)/
	rm -f "$(top_builddir)/vlc-$(VERSION)-rw.dmg"
	hdiutil create -verbose -srcfolder "$(top_builddir)/vlc-$(VERSION)" "$(top_builddir)/vlc-$(VERSION)-rw.dmg" -scrub -format UDRW
	mkdir -p ./mount
	hdiutil attach -readwrite -noverify -noautoopen -mountRoot ./mount "vlc-$(VERSION)-rw.dmg"
	-osascript "$(srcdir)"/extras/package/macosx/dmg_setup.scpt "vlc-$(VERSION)"
	hdiutil detach ./mount/"vlc-$(VERSION)"
# Make sure the image is not writable
# Note: We can't directly create a read only dmg as we do the bless stuff
	rm -f "$(top_builddir)/vlc-$(VERSION).dmg"
	hdiutil convert "$(top_builddir)/vlc-$(VERSION)-rw.dmg" -format UDBZ -o "$(top_builddir)/vlc-$(VERSION).dmg"
	ls -l "$(top_builddir)/vlc-$(VERSION).dmg"
	rm -f "$(top_builddir)/vlc-$(VERSION)-rw.dmg"
	rm -rf "$(top_builddir)/vlc-$(VERSION)"

package-macosx-zip: VLC.app
	mkdir -p $(top_builddir)/vlc-$(VERSION)/Goodies/
	cp -R $(top_builddir)/VLC.app $(top_builddir)/vlc-$(VERSION)/VLC.app
	cd $(srcdir); cp -R AUTHORS COPYING README THANKS NEWS $(abs_top_builddir)/vlc-$(VERSION)/Goodies/
	zip -r -y -9 $(top_builddir)/vlc-$(VERSION).zip $(top_builddir)/vlc-$(VERSION)
	rm -rf "$(top_builddir)/vlc-$(VERSION)"

package-translations:
	mkdir -p "$(srcdir)/vlc-translations-$(VERSION)"
	for i in `cat "$(top_srcdir)/po/LINGUAS"`; do \
	  cp "$(srcdir)/po/$${i}.po" "$(srcdir)/vlc-translations-$(VERSION)/" ; \
	done
	cp "$(srcdir)/doc/translations.txt" "$(srcdir)/vlc-translations-$(VERSION)/README.txt"

	echo "#!/bin/sh" >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo "" >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo 'if test $$# != 1; then' >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo "	echo \"Usage: convert-po.sh <.po file>\"" >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo "	exit 1" >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo "fi" >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo "" >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"
	echo 'msgfmt --statistics -o vlc.mo $$1' >>"$(srcdir)/vlc-translations-$(VERSION)/convert.po.sh"

	$(AMTAR) chof - $(srcdir)/vlc-translations-$(VERSION) \
	  | GZIP=$(GZIP_ENV) gzip -c >$(srcdir)/vlc-translations-$(VERSION).tar.gz

.PHONY: package-macosx package-macosx-zip package-translations

###############################################################################
# Mac OS X project
###############################################################################

EXTRA_DIST += \
	extras/package/macosx/build-package.sh \
	extras/package/macosx/build.sh \
	extras/package/macosx/codesign-dp.sh \
	extras/package/macosx/codesign.sh \
	extras/package/macosx/configure.sh \
	extras/package/macosx/dmg_setup.scpt \
	extras/package/macosx/fullscreen_panel.svg \
	extras/package/macosx/vlc_status_icon.svg \
	extras/package/macosx/Info.plist.in \
	extras/package/macosx/Resources/dsa_pub.pem \
	extras/package/macosx/Resources/English.lproj/About.xib \
	extras/package/macosx/Resources/English.lproj/AddonManager.xib \
	extras/package/macosx/Resources/English.lproj/AudioEffects.xib \
	extras/package/macosx/Resources/English.lproj/Bookmarks.xib \
	extras/package/macosx/Resources/English.lproj/BWQuincyMain.xib \
	extras/package/macosx/Resources/English.lproj/ConvertAndSave.xib \
	extras/package/macosx/Resources/English.lproj/CoreDialogs.xib \
	extras/package/macosx/Resources/English.lproj/DebugMessageVisualizer.xib \
	extras/package/macosx/Resources/English.lproj/DetachedVideoWindow.xib \
	extras/package/macosx/Resources/English.lproj/FSPanel.xib \
	extras/package/macosx/Resources/English.lproj/InfoPlist.strings.in \
	extras/package/macosx/Resources/English.lproj/Help.xib \
	extras/package/macosx/Resources/English.lproj/MainMenu.xib \
	extras/package/macosx/Resources/English.lproj/MainWindow.xib \
	extras/package/macosx/Resources/English.lproj/MediaInfo.xib \
	extras/package/macosx/Resources/English.lproj/Open.xib \
	extras/package/macosx/Resources/English.lproj/PlaylistAccessoryView.xib \
	extras/package/macosx/Resources/English.lproj/PlaylistMenu.xib \
	extras/package/macosx/Resources/English.lproj/PopupPanel.xib \
	extras/package/macosx/Resources/English.lproj/Preferences.xib \
	extras/package/macosx/Resources/English.lproj/ResumeDialog.xib \
	extras/package/macosx/Resources/English.lproj/SimplePreferences.xib \
	extras/package/macosx/Resources/English.lproj/StreamOutput.xib \
	extras/package/macosx/Resources/English.lproj/SyncTracks.xib \
	extras/package/macosx/Resources/English.lproj/TextfieldPanel.xib \
	extras/package/macosx/Resources/English.lproj/TimeSelectionPanel.xib \
	extras/package/macosx/Resources/English.lproj/VideoEffects.xib \
	extras/package/macosx/Resources/English.lproj/VLCStatusBarIconMainMenu.xib \
	extras/package/macosx/Resources/English.lproj/VLCRendererDialog.xib \
	extras/package/macosx/Resources/English.lproj/VLCFullScreenPanel.xib \
	extras/package/macosx/Resources/icons/aiff.icns \
	extras/package/macosx/Resources/icons/audio.icns \
	extras/package/macosx/Resources/icons/avi.icns \
	extras/package/macosx/Resources/icons/flv.icns \
	extras/package/macosx/Resources/icons/generic.icns \
	extras/package/macosx/Resources/icons/m4a.icns \
	extras/package/macosx/Resources/icons/m4v.icns \
	extras/package/macosx/Resources/icons/mkv.icns \
	extras/package/macosx/Resources/icons/mov.icns \
	extras/package/macosx/Resources/icons/movie.icns \
	extras/package/macosx/Resources/icons/mp3.icns \
	extras/package/macosx/Resources/icons/mpeg.icns \
	extras/package/macosx/Resources/icons/ogg.icns \
	extras/package/macosx/Resources/icons/playlist.icns \
	extras/package/macosx/Resources/icons/rm.icns \
	extras/package/macosx/Resources/icons/subtitle.icns \
	extras/package/macosx/Resources/icons/vlc-xmas.icns \
	extras/package/macosx/Resources/icons/vlc.icns \
	extras/package/macosx/Resources/icons/vob.icns \
	extras/package/macosx/Resources/icons/wav.icns \
	extras/package/macosx/Resources/icons/wma.icns \
	extras/package/macosx/Resources/icons/wmv.icns \
	extras/package/macosx/Resources/mainwindow/backward-3btns-pressed.png \
	extras/package/macosx/Resources/mainwindow/backward-3btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/backward-3btns.png \
	extras/package/macosx/Resources/mainwindow/backward-3btns@2x.png \
	extras/package/macosx/Resources/mainwindow/backward-6btns-pressed.png \
	extras/package/macosx/Resources/mainwindow/backward-6btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/backward-6btns.png \
	extras/package/macosx/Resources/mainwindow/backward-6btns@2x.png \
	extras/package/macosx/Resources/mainwindow/bottom-background.png \
	extras/package/macosx/Resources/mainwindow/bottom-background@2x.png \
	extras/package/macosx/Resources/mainwindow/dropzone-background.png \
	extras/package/macosx/Resources/mainwindow/dropzone.png \
	extras/package/macosx/Resources/mainwindow/dropzone@2x.png \
	extras/package/macosx/Resources/mainwindow/effects-double-buttons-pressed.png \
	extras/package/macosx/Resources/mainwindow/effects-double-buttons-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/effects-double-buttons.png \
	extras/package/macosx/Resources/mainwindow/effects-double-buttons@2x.png \
	extras/package/macosx/Resources/mainwindow/effects-one-button-pressed.png \
	extras/package/macosx/Resources/mainwindow/effects-one-button-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/effects-one-button.png \
	extras/package/macosx/Resources/mainwindow/effects-one-button@2x.png \
	extras/package/macosx/Resources/mainwindow/forward-3btns-pressed.png \
	extras/package/macosx/Resources/mainwindow/forward-3btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/forward-3btns.png \
	extras/package/macosx/Resources/mainwindow/forward-3btns@2x.png \
	extras/package/macosx/Resources/mainwindow/forward-6btns-pressed.png \
	extras/package/macosx/Resources/mainwindow/forward-6btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/forward-6btns.png \
	extras/package/macosx/Resources/mainwindow/forward-6btns@2x.png \
	extras/package/macosx/Resources/mainwindow/fullscreen-double-buttons-pressed.png \
	extras/package/macosx/Resources/mainwindow/fullscreen-double-buttons-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/fullscreen-double-buttons.png \
	extras/package/macosx/Resources/mainwindow/fullscreen-double-buttons@2x.png \
	extras/package/macosx/Resources/mainwindow/fullscreen-one-button-pressed.png \
	extras/package/macosx/Resources/mainwindow/fullscreen-one-button-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/fullscreen-one-button.png \
	extras/package/macosx/Resources/mainwindow/fullscreen-one-button@2x.png \
	extras/package/macosx/Resources/mainwindow/next-6btns-pressed.png \
	extras/package/macosx/Resources/mainwindow/next-6btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/next-6btns.png \
	extras/package/macosx/Resources/mainwindow/next-6btns@2x.png \
	extras/package/macosx/Resources/mainwindow/pause-pressed.png \
	extras/package/macosx/Resources/mainwindow/pause-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/pause.png \
	extras/package/macosx/Resources/mainwindow/pause@2x.png \
	extras/package/macosx/Resources/mainwindow/play-pressed.png \
	extras/package/macosx/Resources/mainwindow/play-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/play.png \
	extras/package/macosx/Resources/mainwindow/play@2x.png \
	extras/package/macosx/Resources/mainwindow/playlist-1btn-pressed.png \
	extras/package/macosx/Resources/mainwindow/playlist-1btn-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/playlist-1btn.png \
	extras/package/macosx/Resources/mainwindow/playlist-1btn@2x.png \
	extras/package/macosx/Resources/mainwindow/playlist-btn-pressed.png \
	extras/package/macosx/Resources/mainwindow/playlist-btn-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/playlist-btn.png \
	extras/package/macosx/Resources/mainwindow/playlist-btn@2x.png \
	extras/package/macosx/Resources/mainwindow/previous-6btns-pressed.png \
	extras/package/macosx/Resources/mainwindow/previous-6btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/previous-6btns.png \
	extras/package/macosx/Resources/mainwindow/previous-6btns@2x.png \
	extras/package/macosx/Resources/mainwindow/progression-fill-left.png \
	extras/package/macosx/Resources/mainwindow/progression-fill-left@2x.png \
	extras/package/macosx/Resources/mainwindow/progression-fill-middle.png \
	extras/package/macosx/Resources/mainwindow/progression-fill-middle@2x.png \
	extras/package/macosx/Resources/mainwindow/progression-fill-right.png \
	extras/package/macosx/Resources/mainwindow/progression-fill-right@2x.png \
	extras/package/macosx/Resources/mainwindow/progression-knob.png \
	extras/package/macosx/Resources/mainwindow/progression-knob@2x.png \
	extras/package/macosx/Resources/mainwindow/progression-track-wrapper-left.png \
	extras/package/macosx/Resources/mainwindow/progression-track-wrapper-left@2x.png \
	extras/package/macosx/Resources/mainwindow/progression-track-wrapper-middle.png \
	extras/package/macosx/Resources/mainwindow/progression-track-wrapper-middle@2x.png \
	extras/package/macosx/Resources/mainwindow/progression-track-wrapper-right.png \
	extras/package/macosx/Resources/mainwindow/progression-track-wrapper-right@2x.png \
	extras/package/macosx/Resources/mainwindow/repeat-all-pressed.png \
	extras/package/macosx/Resources/mainwindow/repeat-all-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/repeat-all.png \
	extras/package/macosx/Resources/mainwindow/repeat-all@2x.png \
	extras/package/macosx/Resources/mainwindow/repeat-one-pressed.png \
	extras/package/macosx/Resources/mainwindow/repeat-one-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/repeat-one.png \
	extras/package/macosx/Resources/mainwindow/repeat-one@2x.png \
	extras/package/macosx/Resources/mainwindow/repeat-pressed.png \
	extras/package/macosx/Resources/mainwindow/repeat-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/repeat.png \
	extras/package/macosx/Resources/mainwindow/repeat@2x.png \
	extras/package/macosx/Resources/mainwindow/shuffle-blue-pressed.png \
	extras/package/macosx/Resources/mainwindow/shuffle-blue-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/shuffle-blue.png \
	extras/package/macosx/Resources/mainwindow/shuffle-blue@2x.png \
	extras/package/macosx/Resources/mainwindow/shuffle-pressed.png \
	extras/package/macosx/Resources/mainwindow/shuffle-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/shuffle.png \
	extras/package/macosx/Resources/mainwindow/shuffle@2x.png \
	extras/package/macosx/Resources/mainwindow/stop-pressed.png \
	extras/package/macosx/Resources/mainwindow/stop-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow/stop.png \
	extras/package/macosx/Resources/mainwindow/stop@2x.png \
	extras/package/macosx/Resources/mainwindow/topbar_background.png \
	extras/package/macosx/Resources/mainwindow/volume-high.png \
	extras/package/macosx/Resources/mainwindow/volume-high@2x.png \
	extras/package/macosx/Resources/mainwindow/volume-low.png \
	extras/package/macosx/Resources/mainwindow/volume-low@2x.png \
	extras/package/macosx/Resources/mainwindow/volume-slider-knob.png \
	extras/package/macosx/Resources/mainwindow/volume-slider-knob@2x.png \
	extras/package/macosx/Resources/mainwindow/volume-slider-track.png \
	extras/package/macosx/Resources/mainwindow/volume-slider-track@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/backward-3btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark/backward-3btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/backward-3btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark/backward-3btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/backward-6btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark/backward-6btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/backward-6btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark/backward-6btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/bottom-background_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/bottom-background_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/bottomdark-left.png \
	extras/package/macosx/Resources/mainwindow_dark/bottomdark-left@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/bottomdark-right.png \
	extras/package/macosx/Resources/mainwindow_dark/bottomdark-right@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-double-buttons-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-double-buttons-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-double-buttons_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-double-buttons_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-one-button-pressed-dark.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-one-button-pressed-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-one-button_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/effects-one-button_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/forward-3btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark/forward-3btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/forward-3btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark/forward-3btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/forward-6btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark/forward-6btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/forward-6btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark/forward-6btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/fullscreen-double-buttons-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/fullscreen-double-buttons-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/fullscreen-double-buttons_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/fullscreen-double-buttons_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/fullscreen-one-button-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/fullscreen-one-button-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/fullscreen-one-button_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/fullscreen-one-button_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/next-6btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark/next-6btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/next-6btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark/next-6btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/pause-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/pause-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/pause_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/pause_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/play-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/play-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/play_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/play_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist-1btn-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist-1btn-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist-1btn-dark.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist-1btn-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/playlist_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/previous-6btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark/previous-6btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/previous-6btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark/previous-6btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/progressbar-fill-left_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progressbar-fill-left_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/progressbar-fill-middle_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progressbar-fill-middle_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/progressbar-fill-right_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progressbar-fill-right_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-knob_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-knob_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-track-wrapper-left_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-track-wrapper-left_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-track-wrapper-middle_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-track-wrapper-middle_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-track-wrapper-right_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/progression-track-wrapper-right_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-all-blue-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-all-blue-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-all-blue_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-all-blue_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-one-blue-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-one-blue-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-one-blue_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-one-blue_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/repeat_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle-blue-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle-blue-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle-blue_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle-blue_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/shuffle_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/stop-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/stop-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/stop_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/stop_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-on-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-on@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-over-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close-over@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-close@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen-on@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen-over@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-fullscreen@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-on-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-on@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-over-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize-over@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-minimize@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-on-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-on@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-over-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom-over@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/lion/lion-window-zoom@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-on-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-on@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-over-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close-over@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-close@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-on-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-on@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-over-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen-over@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-fullscreen@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-on-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-on@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-over-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize-over@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-minimize@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-on-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-on-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-on.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-on@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-over-graphite.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-over-graphite@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-over.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom-over@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom.png \
	extras/package/macosx/Resources/mainwindow_dark/titlebar/yosemite/yosemite-window-zoom@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/topbar-dark-center-fill.png \
	extras/package/macosx/Resources/mainwindow_dark/topbar-dark-center-fill@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/topbar-dark-left.png \
	extras/package/macosx/Resources/mainwindow_dark/topbar-dark-left@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/topbar-dark-right.png \
	extras/package/macosx/Resources/mainwindow_dark/topbar-dark-right@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-high_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-high_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-low_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-low_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-slider-knob_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-slider-knob_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-slider-track_dark.png \
	extras/package/macosx/Resources/mainwindow_dark/volume-slider-track_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark/window-resize.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-backward-3btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-backward-3btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-backward-3btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-backward-3btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-backward-6btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-backward-6btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-backward-6btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-backward-6btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-bottom-background_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-bottom-background_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-bottomdark-left.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-bottomdark-left@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-bottomdark-right.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-bottomdark-right@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-effects-double-buttons-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-effects-double-buttons-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-effects-double-buttons_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-effects-double-buttons_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-effects-one-button-pressed-dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-effects-one-button-pressed-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-effects-one-button_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-effects-one-button_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-forward-3btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-forward-3btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-forward-3btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-forward-3btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-forward-6btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-forward-6btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-forward-6btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-forward-6btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-double-buttons-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-double-buttons-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-double-buttons_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-double-buttons_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-one-button-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-one-button-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-one-button_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-fullscreen-one-button_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-next-6btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-next-6btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-next-6btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-next-6btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-pause-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-pause-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-pause_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-pause_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-play-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-play-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-play_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-play_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-playlist-1btn-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-playlist-1btn-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-playlist-1btn-dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-playlist-1btn-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-playlist-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-playlist-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-playlist_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-playlist_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-previous-6btns-dark-pressed.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-previous-6btns-dark-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-previous-6btns-dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-previous-6btns-dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progressbar-fill-left_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progressbar-fill-left_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progressbar-fill-middle_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progressbar-fill-middle_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progressbar-fill-right_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progressbar-fill-right_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progression-knob_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progression-knob_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progression-track-wrapper-left_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progression-track-wrapper-left_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progression-track-wrapper-middle_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progression-track-wrapper-middle_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progression-track-wrapper-right_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-progression-track-wrapper-right_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-all-blue-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-all-blue-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-all-blue_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-all-blue_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-one-blue-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-one-blue-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-one-blue_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-one-blue_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-repeat_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle-blue-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle-blue-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle-blue_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle-blue_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-shuffle_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-stop-pressed_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-stop-pressed_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-stop_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-stop_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-topbar-dark-center-fill.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-topbar-dark-center-fill@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-topbar-dark-left.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-topbar-dark-left@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-topbar-dark-right.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-topbar-dark-right@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-volume-high_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-volume-high_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-volume-low_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-volume-low_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-volume-slider-knob_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-volume-slider-knob_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-volume-slider-track_dark.png \
	extras/package/macosx/Resources/mainwindow_dark_yosemite/ys-volume-slider-track_dark@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-backward-3btns-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-backward-3btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-backward-3btns.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-backward-3btns@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-backward-6btns-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-backward-6btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-backward-6btns.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-backward-6btns@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-bottom-background.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-bottom-background@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-dropzone.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-dropzone@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-effects-double-buttons-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-effects-double-buttons-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-effects-double-buttons.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-effects-double-buttons@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-effects-one-button-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-effects-one-button-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-effects-one-button.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-effects-one-button@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-forward-3btns-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-forward-3btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-forward-3btns.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-forward-3btns@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-forward-6btns-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-forward-6btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-forward-6btns.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-forward-6btns@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-fullscreen-double-buttons-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-fullscreen-double-buttons-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-fullscreen-double-buttons.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-fullscreen-double-buttons@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-fullscreen-one-button-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-fullscreen-one-button-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-fullscreen-one-button.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-fullscreen-one-button@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-next-6btns-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-next-6btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-next-6btns.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-next-6btns@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-pause-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-pause-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-pause.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-pause@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-play-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-play-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-play.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-play@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-playlist-1btn-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-playlist-1btn-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-playlist-1btn.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-playlist-1btn@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-playlist-btn-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-playlist-btn-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-playlist-btn.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-playlist-btn@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-previous-6btns-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-previous-6btns-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-previous-6btns.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-previous-6btns@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-fill-left.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-fill-left@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-fill-middle.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-fill-middle@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-fill-right.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-fill-right@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-knob.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-knob@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-track-wrapper-left.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-track-wrapper-left@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-track-wrapper-middle.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-track-wrapper-middle@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-track-wrapper-right.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-progression-track-wrapper-right@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-all-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-all-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-all.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-all@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-one-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-one-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-one.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-one@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-repeat@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-shuffle-blue-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-shuffle-blue-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-shuffle-blue.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-shuffle-blue@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-shuffle-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-shuffle-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-shuffle.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-shuffle@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-stop-pressed.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-stop-pressed@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-stop.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-stop@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-volume-high.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-volume-high@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-volume-low.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-volume-low@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-volume-slider-knob.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-volume-slider-knob@2x.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-volume-slider-track.png \
	extras/package/macosx/Resources/mainwindow_yosemite/ys-volume-slider-track@2x.png \
	extras/package/macosx/Resources/noart.png \
	extras/package/macosx/Resources/prefs/spref_cone_Audio_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Hotkeys_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Input_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Interface_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Subtitles_64.png \
	extras/package/macosx/Resources/prefs/spref_cone_Video_64.png \
	extras/package/macosx/Resources/README \
	extras/package/macosx/Resources/sidebar-icons/sidebar-local.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-local@2x.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-movie.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-movie@2x.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-music.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-music@2x.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-pictures.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-pictures@2x.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-playlist.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-playlist@2x.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-podcast.png \
	extras/package/macosx/Resources/sidebar-icons/sidebar-podcast@2x.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-local.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-local@2x.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-movie.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-movie@2x.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-music.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-music@2x.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-pictures.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-pictures@2x.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-playlist.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-playlist@2x.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-podcast.png \
	extras/package/macosx/Resources/sidebar-icons_yosemite/ys-sidebar-podcast@2x.png \
	extras/package/macosx/Resources/vlcmenubaricon/VLCNextTemplate.pdf \
   	extras/package/macosx/Resources/vlcmenubaricon/VLCPauseTemplate.pdf \
   	extras/package/macosx/Resources/vlcmenubaricon/VLCPlayTemplate.pdf \
   	extras/package/macosx/Resources/vlcmenubaricon/VLCPreviousTemplate.pdf \
   	extras/package/macosx/Resources/vlcmenubaricon/VLCShuffleTemplate.pdf \
   	extras/package/macosx/Resources/vlcmenubaricon/VLCStatusBarIcon.pdf \
   	extras/package/macosx/Resources/Button-Icons/VLCBackwardTemplate.pdf \
   	extras/package/macosx/Resources/Button-Icons/VLCForwardTemplate.pdf \
   	extras/package/macosx/Resources/Button-Icons/VLCVolumeOnTemplate.pdf \
   	extras/package/macosx/Resources/Button-Icons/VLCVolumeOffTemplate.pdf \
   	extras/package/macosx/Resources/Button-Icons/VLCVolumeOnTemplate.pdf \
   	extras/package/macosx/Resources/Button-Icons/VLCVolumeOffTemplate.pdf \
	extras/package/macosx/Resources/vlc.scriptSuite \
	extras/package/macosx/Resources/vlc.scriptTerminology \
	extras/package/macosx/ub.sh \
	extras/package/macosx/VLC.entitlements \
	extras/package/macosx/vlc.xcodeproj/project.pbxproj


