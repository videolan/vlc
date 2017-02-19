if HAVE_DARWIN
noinst_DATA = pseudo-bundle
endif

# Symlink a pseudo-bundle
pseudo-bundle:
	$(MKDIR_P) $(top_builddir)/bin/Contents/Resources/
	$(LN_S) -hf $(abs_top_builddir)/modules/gui/macosx/UI $(top_builddir)/bin/Contents/Resources/English.lproj
	$(LN_S) -hf $(abs_top_builddir)/share/macosx/Info.plist $(top_builddir)/bin/Contents/Info.plist
	$(LN_S) -hf $(CONTRIB_DIR)/Frameworks
	cd $(top_builddir)/bin/Contents/Resources/ && find $(abs_top_srcdir)/modules/gui/macosx/Resources/ -type f -exec $(LN_S) -f {} \;

# VLC.app for packaging and giving it to your friends
# use package-macosx to get a nice dmg
VLC.app: install
	rm -Rf $@
	## Copy Contents
	cp -R $(prefix)/share/macosx/ $@
	## Copy .strings file
	cp -R $(top_builddir)/modules/gui/macosx/UI/InfoPlist.strings $@/Contents/Resources/UI
	## Rename interface files (UI) folder
	mv $@/Contents/Resources/UI $@/Contents/Resources/English.lproj
	## Copy Info.plist and convert to binary
	cp -R $(top_builddir)/share/macosx/Info.plist $@/Contents/
	xcrun plutil -convert binary1 $@/Contents/Info.plist
	## Create Frameworks dir and copy required ones
	mkdir -p $@/Contents/Frameworks
	cp -R $(CONTRIB_DIR)/Frameworks/Growl.framework $@/Contents/Frameworks
if HAVE_SPARKLE
	cp -R $(CONTRIB_DIR)/Frameworks/Sparkle.framework $@/Contents/Frameworks
endif
if HAVE_BREAKPAD
	cp -R $(CONTRIB_DIR)/Frameworks/Breakpad.framework $@/Contents/Frameworks
endif
	mkdir -p $@/Contents/MacOS/share/locale/
if BUILD_LUA
	## Copy lua scripts
	cp -r "$(prefix)/lib/vlc/lua" "$(prefix)/share/vlc/lua" $@/Contents/MacOS/share/
endif
	## Copy some other stuff (?)
	mkdir -p $@/Contents/MacOS/include/
	(cd "$(prefix)/include" && $(AMTAR) -c --exclude "plugins" vlc) | $(AMTAR) -x -C $@/Contents/MacOS/include/
	## Copy translations
	cat $(top_srcdir)/po/LINGUAS | while read i; do \
	  $(INSTALL) -d $@/Contents/MacOS/share/locale/$${i}/LC_MESSAGES ; \
	  $(INSTALL) $(srcdir)/po/$${i}.gmo $@/Contents/MacOS/share/locale/$${i}/LC_MESSAGES/vlc.mo; \
	  mkdir -p $@/Contents/Resources/$${i}.lproj/ ; \
	  $(LN_S) -f ../English.lproj/InfoPlist.strings ../English.lproj/MainMenu.nib \
		$@/Contents/Resources/$${i}.lproj/ ; \
	done
	printf "APPLVLC#" >| $@/Contents/PkgInfo
	PRODUCT="$@" ACTION="release-makefile" src_dir=$(srcdir) build_dir=$(top_builddir) sh $(srcdir)/extras/package/macosx/build-package.sh
	## Generate plugin cache
	bin/vlc-cache-gen $@/Contents/MacOS/plugins
	find $@ -type d -exec chmod ugo+rx '{}' \;
	find $@ -type f -exec chmod ugo+r '{}' \;


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

.PHONY: package-macosx package-macosx-zip package-translations pseudo-bundle

###############################################################################
# Mac OS X project
###############################################################################

EXTRA_DIST += \
	extras/package/macosx/build-package.sh \
	extras/package/macosx/build.sh \
	extras/package/macosx/codesign.sh \
	extras/package/macosx/configure.sh \
	extras/package/macosx/dmg_setup.scpt \
	extras/package/macosx/fullscreen_panel.svg \
	extras/package/macosx/vlc_status_icon.svg \
	extras/package/macosx/vlc_app_icon.svg \
	extras/package/macosx/VLC.entitlements \
	extras/package/macosx/VLC.xcodeproj/project.pbxproj
