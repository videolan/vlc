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
	## Copy .strings file and .nib files
	cp -R $(top_builddir)/modules/gui/macosx/UI $@/Contents/Resources/English.lproj
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
	cp -r "$(prefix)/share/vlc/lua" $@/Contents/MacOS/share/
	cp -r "$(prefix)/lib/vlc/lua" $@/Contents/MacOS/share/
endif
	## HRTFs
	cp -r $(srcdir)/share/hrtfs $@/Contents/MacOS/share/
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
	## Copy libs
	mkdir -p $@/Contents/MacOS/lib
	find $(prefix)/lib -name 'libvlc*.dylib' -maxdepth 1 -exec cp -a {} $@/Contents/MacOS/lib \;
	## Copy plugins
	mkdir -p $@/Contents/MacOS/plugins
	find $(prefix)/lib/vlc/plugins -name 'lib*_plugin.dylib' -maxdepth 2 -exec cp -a {} $@/Contents/MacOS/plugins \;
	## Copy libbluray jar
	find "$(CONTRIB_DIR)/share/java/" -name 'libbluray-j2se-*.jar' -maxdepth 1 -exec cp -a {} $@/Contents/MacOS/plugins \;
	## Install binary
	cp $(prefix)/bin/vlc $@/Contents/MacOS/VLC
	## Generate plugin cache
	bin/vlc-cache-gen $@/Contents/MacOS/plugins
	find $@ -type d -exec chmod ugo+rx '{}' \;
	find $@ -type f -exec chmod ugo+r '{}' \;


package-macosx: VLC.app
	rm -f "$(top_builddir)/vlc-$(VERSION).dmg"
if HAVE_DMGBUILD
	@echo "Packaging fancy DMG using dmgbuild"
	cd "$(top_srcdir)/extras/package/macosx/dmg" && dmgbuild -s "dmg_settings.py" \
		-D app="$(abs_top_builddir)/VLC.app" "VLC Media Player" "$(abs_top_builddir)/vlc-$(VERSION).dmg"
else !HAVE_DMGBUILD
	@echo "Packaging non-fancy DMG"
	## Create directory for DMG contents
	mkdir -p "$(top_builddir)/vlc-$(VERSION)"
	## Copy contents
	cp -Rp "$(top_builddir)/VLC.app" "$(top_builddir)/vlc-$(VERSION)/VLC.app"
	## Symlink to Applications so users can easily drag-and-drop the App to it
	$(LN_S) -f /Applications "$(top_builddir)/vlc-$(VERSION)/"
	## Create DMG
	hdiutil create -srcfolder "$(top_builddir)/vlc-$(VERSION)" -volname "VLC Media Player" \
		-format UDBZ -o "$(top_builddir)/vlc-$(VERSION).dmg"
	## Cleanup
	rm -rf "$(top_builddir)/vlc-$(VERSION)"
endif

package-macosx-zip: VLC.app
	rm -f "$(top_builddir)/vlc-$(VERSION).zip"
	mkdir -p $(top_builddir)/vlc-$(VERSION)/Goodies/
	cp -Rp $(top_builddir)/VLC.app $(top_builddir)/vlc-$(VERSION)/VLC.app
	cd $(srcdir); cp -R AUTHORS COPYING README THANKS NEWS $(abs_top_builddir)/vlc-$(VERSION)/Goodies/
	zip -r -y -9 $(top_builddir)/vlc-$(VERSION).zip $(top_builddir)/vlc-$(VERSION)
	rm -rf "$(top_builddir)/vlc-$(VERSION)"

package-macosx-release:
	rm -f "$(top_builddir)/vlc-$(VERSION)-release.zip"
	mkdir -p $(top_builddir)/vlc-$(VERSION)-release
	cp -Rp $(top_builddir)/VLC.app $(top_builddir)/vlc-$(VERSION)-release/
	cp $(srcdir)/extras/package/macosx/dmg/* $(top_builddir)/vlc-$(VERSION)-release/
	cp "$(srcdir)/extras/package/macosx/codesign.sh" $(top_builddir)/vlc-$(VERSION)-release/
	cp "$(prefix)/lib/vlc/vlc-cache-gen" $(top_builddir)/vlc-$(VERSION)-release/
	install_name_tool -add_rpath "@executable_path/VLC.app/Contents/MacOS/lib" $(top_builddir)/vlc-$(VERSION)-release/vlc-cache-gen
	zip -r -y -9 $(top_builddir)/vlc-$(VERSION)-release.zip $(top_builddir)/vlc-$(VERSION)-release
	rm -rf "$(top_builddir)/vlc-$(VERSION)-release"

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

.PHONY: package-macosx package-macosx-zip package-macosx-release package-translations pseudo-bundle

###############################################################################
# Mac OS X project
###############################################################################

EXTRA_DIST += \
	extras/package/macosx/build.sh \
	extras/package/macosx/codesign.sh \
	extras/package/macosx/configure.sh \
	extras/package/macosx/dmg/dmg_settings.py \
	extras/package/macosx/dmg/disk_image.icns \
	extras/package/macosx/dmg/background.tiff \
	extras/package/macosx/asset_sources/vlc_app_icon.svg \
	extras/package/macosx/VLC.entitlements \
	extras/package/macosx/VLC.xcodeproj/project.pbxproj
